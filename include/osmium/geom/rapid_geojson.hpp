#ifndef OSMIUM_GEOM_RAPID_GEOJSON_HPP
#define OSMIUM_GEOM_RAPID_GEOJSON_HPP

/*

This file is part of Osmium (http://osmcode.org/libosmium).

Copyright 2013-2017 Jochen Topf <jochen@topf.org> and others (see README).

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

#include <cstddef>

#include <osmium/geom/coordinates.hpp>
#include <osmium/geom/factory.hpp>
#include "rapidjson/document.h"

namespace osmium {

    namespace geom {

        namespace detail {

            /**
             * A geometry factory implementation that can be used with the
             * RapidJSON (https://github.com/miloyip/rapidjson) JSON writer.
             */
            template <typename TWriter>
            class RapidGeoJSONFactoryImpl {

                TWriter* m_writer;

            public:

                // using point_type        = void;
                // using linestring_type   = void;
                // using polygon_type      = void;
                // using multipolygon_type = void;
                // using ring_type         = void;

                using point_type        = rapidjson::Document;
                using linestring_type   = rapidjson::Document;
                using polygon_type      = rapidjson::Document;
                using multipolygon_type = rapidjson::Document;
                using ring_type         = rapidjson::Document;

                RapidGeoJSONFactoryImpl(int /* srid */, TWriter& writer) :
                    m_writer(&writer) {
                }

                /* Point */

                // { "type": "Point", "coordinates": [100.0, 0.0] }
                point_type make_point(const osmium::geom::Coordinates& xy) const {
                    rapidjson::Document document;
                    document.SetObject();
                    rapidjson::Document::AllocatorType& allocator = document.GetAllocator();

                    document.AddMember("type", "Point", allocator);

                    rapidjson::Value coordinates(rapidjson::kArrayType);
                    coordinates.PushBack(xy.x, allocator);
                    coordinates.PushBack(xy.y, allocator);

                    document.AddMember("coordinates", coordinates, allocator);

                    return document;
                }

                /* LineString */

                // { "type": "LineString", "coordinates": [ [100.0, 0.0], [101.0, 1.0] ] }
                void linestring_start() {
                    m_writer->StartObject();
                    m_writer->String("type");
                    m_writer->String("LineString");
                    m_writer->String("coordinates");
                    m_writer->StartArray();
                }

                void linestring_add_location(const osmium::geom::Coordinates& xy) {
                    m_writer->StartArray();
                    m_writer->Double(xy.x);
                    m_writer->Double(xy.y);
                    m_writer->EndArray();
                }

                linestring_type linestring_finish(size_t /* num_points */) {
                    m_writer->EndArray();
                    m_writer->EndObject();
                }

                /* Polygon */

                // { "type": "Polygon", "coordinates": [[[100.0, 0.0], [101.0, 1.0]]] }
                void polygon_start() {
                    m_writer->StartObject();
                    m_writer->String("type");
                    m_writer->String("Polygon");
                    m_writer->String("coordinates");
                    m_writer->StartArray();
                    m_writer->StartArray();
                }

                void polygon_add_location(const osmium::geom::Coordinates& xy) {
                    m_writer->StartArray();
                    m_writer->Double(xy.x);
                    m_writer->Double(xy.y);
                    m_writer->EndArray();
                }

                polygon_type polygon_finish(size_t /* num_points */) {
                    m_writer->EndArray();
                    m_writer->EndArray();
                    m_writer->EndObject();
                }

                /* MultiPolygon */

                void multipolygon_start() {
                    m_writer->StartObject();
                    m_writer->String("type");
                    m_writer->String("MultiPolygon");
                    m_writer->String("coordinates");
                    m_writer->StartArray();
                }

                void multipolygon_polygon_start() {
                    m_writer->StartArray();
                }

                void multipolygon_polygon_finish() {
                    m_writer->EndArray();
                }

                void multipolygon_outer_ring_start() {
                    m_writer->StartArray();
                }

                void multipolygon_outer_ring_finish() {
                    m_writer->EndArray();
                }

                void multipolygon_inner_ring_start() {
                    m_writer->StartArray();
                }

                void multipolygon_inner_ring_finish() {
                    m_writer->EndArray();
                }

                void multipolygon_add_location(const osmium::geom::Coordinates& xy) {
                    m_writer->StartArray();
                    m_writer->Double(xy.x);
                    m_writer->Double(xy.y);
                    m_writer->EndArray();
                }

                multipolygon_type multipolygon_finish() {
                    m_writer->EndArray();
                    m_writer->EndObject();
                }

            }; // class RapidGeoJSONFactoryImpl

        } // namespace detail

        template <typename TWriter, typename TProjection = IdentityProjection>
        using RapidGeoJSONFactory = GeometryFactory<detail::RapidGeoJSONFactoryImpl<TWriter>, TProjection>;

    } // namespace geom

} // namespace osmium

#endif // OSMIUM_GEOM_RAPID_GEOJSON_HPP
