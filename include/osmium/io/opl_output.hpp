#ifndef OSMIUM_IO_OPL_OUTPUT_HPP
#define OSMIUM_IO_OPL_OUTPUT_HPP

/*

This file is part of Osmium (http://osmcode.org/osmium).

Copyright 2013 Jochen Topf <jochen@topf.org> and others (see README).

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

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <sstream>
#include <iomanip>

#include <osmium/io/output.hpp>
#include <osmium/handler.hpp>
#include <osmium/utils/timestamp.hpp>

namespace osmium {

    namespace io {

        class OPLOutput : public osmium::io::Output, public osmium::handler::Handler<OPLOutput> {

            // XXX it is very inefficient to use a stringstream here and write it out after each line,
            // but currently this is the easiest way to make it work with bz2/gz compression.
            std::stringstream m_out;

            OPLOutput(const OPLOutput&) = delete;
            OPLOutput& operator=(const OPLOutput&) = delete;

        public:

            OPLOutput(const osmium::io::File& file) :
                Output(file),
                m_out() {
            }

            void handle_collection(osmium::memory::Buffer::const_iterator begin, osmium::memory::Buffer::const_iterator end) override {
                this->operator()(begin, end);
            }

            void set_header(osmium::io::Header&) override {
            }

            void node(const osmium::Node& node) {
                m_out << "n";
                write_meta(node);
                m_out << " x"
                      << node.lon()
                      << " y"
                      << node.lat();
                write_tags(node.tags());
                m_out << "\n";
                ::write(this->fd(), m_out.str().c_str(), m_out.str().size());
                m_out.str("");
            }

            void way(const osmium::Way& way) {
                m_out << "w";
                write_meta(way);

                m_out << " N";
                int n=0;
                for (const auto& wn : way.nodes()) {
                    if (n++ != 0) {
                        m_out << ",";
                    }
                    m_out << "n"
                          << wn.ref();
                }

                write_tags(way.tags());
                m_out << "\n";
                ::write(this->fd(), m_out.str().c_str(), m_out.str().size());
                m_out.str("");
            }

            void relation(const osmium::Relation& relation) {
                m_out << "r";
                write_meta(relation);

                m_out << " M";
                int n=0;
                for (const auto& member : relation.members()) {
                    if (n++ != 0) {
                        m_out << ",";
                    }
                    m_out << item_type_to_char(member.type())
                          << member.ref()
                          << "!"
                          << member.role();
                }

                write_tags(relation.tags());
                m_out << "\n";
                ::write(this->fd(), m_out.str().c_str(), m_out.str().size());
                m_out.str("");
            }

            void close() override {
            }

        private:

            std::string encode(const std::string& data) const {
                static constexpr char hex[] = "0123456789abcdef";

                std::string buffer;
                buffer.reserve(data.size());
                for (size_t pos = 0; pos != data.size(); ++pos) {
                    char c = data[pos];
                    if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == ':' || c == ';') {
                        buffer.append(1, c);
                    } else {
                        buffer.append(1, '%');
                        buffer.append(1, hex[(c & 0xf0) >> 4]);
                        buffer.append(1, hex[c & 0x0f]);
                    }
                }
                return std::move(buffer);
            }

            void write_meta(const osmium::Object& object) {
                m_out << object.id()
                      << " v"
                      << object.version()
                      << " V"
                      << (object.visible() ? 't' : 'f')
                      << " c"
                      << object.changeset()
                      << " t"
                      << timestamp::to_iso(object.timestamp())
                      << " i"
                      << object.uid()
                      << " u"
                      << encode(object.user());
            }

            void write_tags(const osmium::TagList& tags) {
                m_out << " T";
                int n=0;
                for (auto& tag : tags) {
                    if (n++ != 0) {
                        m_out << ",";
                    }
                    m_out << encode(tag.key())
                          << "="
                          << encode(tag.value());
                }
            }

        }; // class OPLOutput

        namespace {

            const bool registered_opl_output = osmium::io::OutputFactory::instance().register_output_format({
                osmium::io::Encoding::OPL(),
                osmium::io::Encoding::OPLgz(),
                osmium::io::Encoding::OPLbz2()
            }, [](const osmium::io::File& file) {
                return new osmium::io::OPLOutput(file);
            });

        } // anonymous namespace

    } // namespace io

} // namespace osmium

#endif // OSMIUM_IO_OPL_OUTPUT_HPP
