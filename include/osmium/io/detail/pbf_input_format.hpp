#ifndef OSMIUM_IO_DETAIL_PBF_INPUT_FORMAT_HPP
#define OSMIUM_IO_DETAIL_PBF_INPUT_FORMAT_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2016 Jochen Topf <jochen@topf.org> and others (see README).

Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization
obtaining a copy of the software and accompanying documentation covered by
this license (the "Software") to use, reproduce, display, distribute,
execute, and transmit the Software, and to prepare derivative works of the
Software, and to permit third-parties to whom the Software is furnished to
do so, all subject to the following:

The copyright notices in the Software and this entire statement, including
the above license grant, this restriction and the following disclaimer,
must be included in all copies of the Software, in whole or in part, and
all derivative works of the Software, unless such copies or derivative
works are solely in the form of machine-executable object code generated by
a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
DEALINGS IN THE SOFTWARE.

*/

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <type_traits>

#include <protozero/pbf_message.hpp>
#include <protozero/types.hpp>

#include <osmium/io/detail/input_format.hpp>
#include <osmium/io/detail/pbf.hpp> // IWYU pragma: export
#include <osmium/io/detail/pbf_decoder.hpp>
#include <osmium/io/detail/protobuf_tags.hpp>
#include <osmium/io/detail/queue_util.hpp>
#include <osmium/io/file_format.hpp>
#include <osmium/io/header.hpp>
#include <osmium/osm/entity_bits.hpp>
#include <osmium/thread/pool.hpp>
#include <osmium/thread/util.hpp>
#include <osmium/util/config.hpp>

namespace osmium {

    namespace io {

        namespace detail {

            class PBFParser : public Parser {

                std::string m_input_buffer;

                /**
                 * Read the given number of bytes from the input queue.
                 *
                 * @param size Number of bytes to read
                 * @returns String with the data
                 * @throws osmium::pbf_error If size bytes can't be read
                 */
                std::string read_from_input_queue(size_t size) {
                    while (m_input_buffer.size() < size) {
                        const std::string new_data = get_input();
                        if (input_done()) {
                            throw osmium::pbf_error("truncated data (EOF encountered)");
                        }
                        m_input_buffer += new_data;
                    }

                    std::string output { m_input_buffer.substr(size) };
                    m_input_buffer.resize(size);

                    using std::swap;
                    swap(output, m_input_buffer);

                    return output;
                }

                /**
                 * Read 4 bytes in network byte order from file. They contain
                 * the length of the following BlobHeader.
                 */
                uint32_t read_blob_header_size_from_file() {
                    uint32_t size_in_network_byte_order;

                    try {
                        const std::string input_data = read_from_input_queue(sizeof(size_in_network_byte_order));
                        size_in_network_byte_order = *reinterpret_cast<const uint32_t*>(input_data.data());
                    } catch (const osmium::pbf_error&) {
                        return 0; // EOF
                    }

                    const uint32_t size = ntohl(size_in_network_byte_order);
                    if (size > static_cast<uint32_t>(max_blob_header_size)) {
                        throw osmium::pbf_error("invalid BlobHeader size (> max_blob_header_size)");
                    }

                    return size;
                }

                /**
                 * Decode the BlobHeader. Make sure it contains the expected
                 * type. Return the size of the following Blob.
                 */
                size_t decode_blob_header(protozero::pbf_message<FileFormat::BlobHeader>&& pbf_blob_header, const char* expected_type) {
                    protozero::data_view blob_header_type;
                    size_t blob_header_datasize = 0;

                    while (pbf_blob_header.next()) {
                        switch (pbf_blob_header.tag()) {
                            case FileFormat::BlobHeader::required_string_type:
                                blob_header_type = pbf_blob_header.get_view();
                                break;
                            case FileFormat::BlobHeader::required_int32_datasize:
                                blob_header_datasize = pbf_blob_header.get_int32();
                                break;
                            default:
                                pbf_blob_header.skip();
                        }
                    }

                    if (blob_header_datasize == 0) {
                        throw osmium::pbf_error("PBF format error: BlobHeader.datasize missing or zero.");
                    }

                    if (std::strncmp(expected_type, blob_header_type.data(), blob_header_type.size())) {
                        throw osmium::pbf_error("blob does not have expected type (OSMHeader in first blob, OSMData in following blobs)");
                    }

                    return blob_header_datasize;
                }

                size_t check_type_and_get_blob_size(const char* expected_type) {
                    assert(expected_type);

                    const auto size = read_blob_header_size_from_file();
                    if (size == 0) { // EOF
                        return 0;
                    }

                    const std::string blob_header = read_from_input_queue(size);

                    return decode_blob_header(protozero::pbf_message<FileFormat::BlobHeader>(blob_header), expected_type);
                }

                std::string read_from_input_queue_with_check(size_t size) {
                    if (size > max_uncompressed_blob_size) {
                        throw osmium::pbf_error(std::string("invalid blob size: " +
                                                std::to_string(size)));
                    }
                    return read_from_input_queue(size);
                }

                // Parse the header in the PBF OSMHeader blob.
                void parse_header_blob() {
                    osmium::io::Header header;
                    const auto size = check_type_and_get_blob_size("OSMHeader");
                    header = decode_header(read_from_input_queue_with_check(size));
                    set_header_value(header);
                }

                void parse_data_blobs() {
                    while (const auto size = check_type_and_get_blob_size("OSMData")) {
                        std::string input_buffer = read_from_input_queue_with_check(size);

                        PBFDataBlobDecoder data_blob_parser{std::move(input_buffer), read_types(), read_metadata()};

                        if (osmium::config::use_pool_threads_for_pbf_parsing()) {
                            send_to_output_queue(osmium::thread::Pool::instance().submit(std::move(data_blob_parser)));
                        } else {
                            send_to_output_queue(data_blob_parser());
                        }
                    }
                }

            public:

                PBFParser(future_string_queue_type& input_queue,
                          future_buffer_queue_type& output_queue,
                          std::promise<osmium::io::Header>& header_promise,
                          osmium::osm_entity_bits::type read_types,
                          osmium::io::read_meta read_metadata) :
                    Parser(input_queue, output_queue, header_promise, read_types, read_metadata),
                    m_input_buffer() {
                }

                ~PBFParser() noexcept final = default;

                void run() final {
                    osmium::thread::set_thread_name("_osmium_pbf_in");

                    parse_header_blob();

                    if (read_types() != osmium::osm_entity_bits::nothing) {
                        parse_data_blobs();
                    }
                }

            }; // class PBFParser

            // we want the register_parser() function to run, setting
            // the variable is only a side-effect, it will never be used
            const bool registered_pbf_parser = ParserFactory::instance().register_parser(
                file_format::pbf,
                [](future_string_queue_type& input_queue,
                    future_buffer_queue_type& output_queue,
                    std::promise<osmium::io::Header>& header_promise,
                    osmium::osm_entity_bits::type read_which_entities,
                    osmium::io::read_meta read_metadata) {
                    return std::unique_ptr<Parser>(new PBFParser(input_queue, output_queue, header_promise, read_which_entities, read_metadata));
            });

            // dummy function to silence the unused variable warning from above
            inline bool get_registered_pbf_parser() noexcept {
                return registered_pbf_parser;
            }

        } // namespace detail

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_DETAIL_PBF_INPUT_FORMAT_HPP
