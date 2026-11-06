#include "ripper/pdf/core/util/indirect.hpp"

#include <catch2/catch_test_macros.hpp>
#include <string>
#include <utility>

namespace ripper::pdf::core
{
namespace
{
/// `node` uses `indirect<node>` while `node` is still incomplete, which is the
/// property `object` relies on for its dictionary and stream alternatives.
struct node
{
    int value = 0;
    indirect<node> next;

    bool operator==(const node& other) const noexcept;
};

bool node::operator==(const node& other) const noexcept
{
    return value == other.value && next == other.next;
}

node make_chain()
{
    const node third{3, indirect<node>{}};
    const node second{2, indirect<node>{third}};
    return node{1, indirect<node>{second}};
}

/// `forward_only` is used through `indirect` before it is defined.
struct forward_only;

indirect<forward_only> make_forward_only();

struct forward_only
{
    int number = 0;

    bool operator==(const forward_only& other) const noexcept
    {
        return number == other.number;
    }
};

indirect<forward_only> make_forward_only()
{
    return indirect<forward_only>{forward_only{42}};
}
} // namespace

TEST_CASE("indirect is empty by default", "[util][indirect]")
{
    const indirect<int> empty;

    REQUIRE(empty.get() == nullptr);
    REQUIRE(empty == nullptr);
    REQUIRE_FALSE(static_cast<bool>(empty));
}

TEST_CASE("indirect owns a value", "[util][indirect]")
{
    const indirect<int> value{7};

    REQUIRE(value.get() != nullptr);
    REQUIRE(*value == 7);
    REQUIRE(static_cast<bool>(value));
}

TEST_CASE("indirect copies deeply", "[util][indirect]")
{
    const indirect<node> original{node{1, indirect<node>{node{2, indirect<node>{}}}}};
    indirect<node> copy{original};

    REQUIRE(copy == original);
    REQUIRE(copy.get() != original.get());
    REQUIRE(copy->next.get() != original->next.get());

    copy->next->value = 99;

    REQUIRE(copy != original);
    REQUIRE(original->next->value == 2);
}

TEST_CASE("indirect copy assignment keeps prior value on failure paths", "[util][indirect]")
{
    indirect<node> lhs{node{1, indirect<node>{}}};
    const indirect<node> rhs{node{2, indirect<node>{node{3, indirect<node>{}}}}};

    lhs = rhs;
    REQUIRE(lhs == rhs);
    REQUIRE(lhs.get() != rhs.get());

    lhs = lhs;
    REQUIRE(lhs == rhs);

    lhs = indirect<node>{};
    REQUIRE(lhs == nullptr);
}

TEST_CASE("indirect move transfers ownership and empties the source", "[util][indirect]")
{
    indirect<node> source{node{5, indirect<node>{}}};
    const node* address = source.get();

    indirect<node> destination{std::move(source)};

    REQUIRE(source == nullptr);
    REQUIRE(destination.get() == address);
    REQUIRE(destination->value == 5);

    indirect<node> assigned;
    assigned = std::move(destination);

    REQUIRE(destination == nullptr);
    REQUIRE(assigned->value == 5);
}

TEST_CASE("indirect equality compares pointees", "[util][indirect]")
{
    const indirect<int> lhs{4};
    const indirect<int> rhs{4};
    const indirect<int> other{5};

    REQUIRE(lhs == rhs);
    REQUIRE(lhs != other);
    REQUIRE(other != nullptr);

    REQUIRE(indirect<int>{} == indirect<int>{});
    REQUIRE(indirect<int>{} != lhs);
}

TEST_CASE("indirect supports recursive aggregates", "[util][indirect]")
{
    const node chain = make_chain();

    REQUIRE(chain.value == 1);
    REQUIRE(chain.next->value == 2);
    REQUIRE(chain.next->next->value == 3);
    REQUIRE(chain.next->next->next == nullptr);

    node copy = chain;
    REQUIRE(copy == chain);
    REQUIRE(copy.next.get() != chain.next.get());

    node moved{std::move(copy)};
    REQUIRE(moved == chain);
    REQUIRE(copy.next == nullptr);
}

TEST_CASE("indirect supports incomplete types", "[util][indirect]")
{
    const indirect<forward_only> value = make_forward_only();

    REQUIRE(value->number == 42);
    REQUIRE(value == indirect<forward_only>{forward_only{42}});
    REQUIRE(value != indirect<forward_only>{forward_only{43}});
}

} // namespace ripper::pdf::core
