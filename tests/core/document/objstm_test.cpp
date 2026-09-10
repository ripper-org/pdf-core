#include "ripper/pdf/core/document.hpp"
#include "ripper/pdf/core/document/object/helpers/indirect_object.hpp"
#include "ripper/pdf/core/document/object/object.hpp"
#include "ripper/pdf/core/document/objstm.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <memory>
#include <string_view>
#include <vector>

namespace ripper::pdf::core
{
namespace
{
std::vector<std::byte> b(std::string_view sv)
{
    std::vector<std::byte> v;
    v.reserve(sv.size());
    for (char c : sv)
        v.push_back(static_cast<std::byte>(c));
    return v;
}

std::unique_ptr<indirect_object> make_objstm_object(document& doc, indirect_reference ref,
                                                    dictionary_object dict,
                                                    std::vector<std::byte> payload)
{
    dict.set("Type", object{name_object{"ObjStm"}});
    auto st = stream_object{std::move(dict), stream{std::move(payload)}};
    return std::make_unique<indirect_object>(object_identity{&doc, ref}, object{std::move(st)});
}
} // namespace

TEST_CASE("objstm reads /N and /First from the stream dictionary",
          "[document][objstm][count][first_offset]")
{
    document doc{nullptr, nullptr};
    indirect_reference ref{7, 0};

    dictionary_object dict;
    dict.set("N", object{static_cast<std::int64_t>(2)});
    dict.set("First", object{static_cast<std::int64_t>(8)});

    auto obj = make_objstm_object(doc, ref, std::move(dict), b("1 0\n2 7\nAAAAAAAB"));

    objstm view{*obj};
    REQUIRE(view.count() == 2);
    REQUIRE(view.first_offset() == 8);
}

TEST_CASE("objstm object_offset parses header and returns byte ranges",
          "[document][objstm][object_offset]")
{
    document doc{nullptr, nullptr};
    indirect_reference ref{7, 0};

    dictionary_object dict;
    dict.set("N", object{static_cast<std::int64_t>(2)});
    dict.set("First", object{static_cast<std::int64_t>(8)});

    auto obj = make_objstm_object(doc, ref, std::move(dict), b("1 0\n2 7\nAAAAAAAB"));

    objstm view{*obj};

    const auto first = view.object_offset(0);
    REQUIRE(first.has_value());
    REQUIRE(first->offset == 8);
    REQUIRE(first->length == 7);

    const auto second = view.object_offset(1);
    REQUIRE(second.has_value());
    REQUIRE(second->offset == 15);
    REQUIRE(second->length == 1);

    REQUIRE_FALSE(view.object_offset(2).has_value());
}

TEST_CASE("objstm extension is nullopt without /Extends and throws on self reference",
          "[document][objstm][extension]")
{
    document doc{nullptr, nullptr};
    indirect_reference ref{7, 0};

    dictionary_object dict;
    dict.set("N", object{static_cast<std::int64_t>(1)});
    dict.set("First", object{static_cast<std::int64_t>(1)});

    auto obj = make_objstm_object(doc, ref, std::move(dict), b("1 0\nA"));
    objstm view{*obj};
    REQUIRE_FALSE(view.extension().has_value());

    dictionary_object cyclic;
    cyclic.set("N", object{static_cast<std::int64_t>(1)});
    cyclic.set("First", object{static_cast<std::int64_t>(1)});
    cyclic.set("Extends", object{indirect_reference{7, 0}});

    auto obj2 = make_objstm_object(doc, ref, std::move(cyclic), b("1 0\nA"));
    objstm view2{*obj2};
    REQUIRE_THROWS_AS(view2.extension(), parse_exception);
}

TEST_CASE("objstm over a non-stream value throws a parse exception",
          "[document][objstm][validation]")
{
    document doc{nullptr, nullptr};
    indirect_reference ref{7, 0};

    auto obj = std::make_unique<indirect_object>(object_identity{&doc, ref},
                                                 object{std::string{"not a stream"}});

    objstm view{*obj};
    REQUIRE_THROWS_AS(view.count(), parse_exception);
}
} // namespace ripper::pdf::core