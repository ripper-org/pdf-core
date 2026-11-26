#include "ripper/pdf/core/document/graphics/text/text.hpp"

#include "ripper/pdf/core/document/catalog/pages/page/content/operator.hpp"
#include "ripper/pdf/core/document/graphics/brush/brush.hpp"
#include "ripper/pdf/core/exceptions/exception.hpp"

#include <string>
#include <utility>

namespace ripper::pdf::core::graphics
{
text& text::set_font(std::string_view font_name, double size)
{
    ensure_active();
    brush_->set_font(font_name, size);
    return *this;
}

text& text::set_character_spacing(double spacing)
{
    ensure_active();
    brush_->set_character_spacing(spacing);
    return *this;
}

text& text::set_word_spacing(double spacing)
{
    ensure_active();
    brush_->set_word_spacing(spacing);
    return *this;
}

text& text::set_horizontal_scaling(double percent)
{
    ensure_active();
    brush_->set_horizontal_scaling(percent);
    return *this;
}

text& text::set_leading(double leading)
{
    ensure_active();
    brush_->set_leading(leading);
    return *this;
}

text& text::set_text_rise(double rise)
{
    ensure_active();
    brush_->set_text_rise(rise);
    return *this;
}

text& text::set_text_render_mode(text_render_mode mode)
{
    ensure_active();
    brush_->set_text_render_mode(mode);
    return *this;
}

text& text::move_by(double tx, double ty)
{
    ensure_active();
    brush_->op(content_operator::Td, {object{tx}, object{ty}});
    return *this;
}

text& text::move_by_and_set_leading(double tx, double ty)
{
    ensure_active();
    brush_->op(content_operator::TD, {object{tx}, object{ty}});
    return *this;
}

text& text::next_line()
{
    ensure_active();
    brush_->op(content_operator::T_star);
    return *this;
}

text& text::set_matrix(const geometry::matrix& transformation)
{
    ensure_active();
    brush_->op(content_operator::Tm,
               {object{transformation.a}, object{transformation.b}, object{transformation.c},
                object{transformation.d}, object{transformation.e}, object{transformation.f}});
    return *this;
}

text& text::show(std::string_view encoded_text)
{
    ensure_active();
    brush_->op(content_operator::Tj, {object{std::string{encoded_text}}});
    return *this;
}

text& text::show(std::span<const text_show_item> items)
{
    ensure_active();

    array_object::storage_type values;
    values.reserve(items.size());

    for (const auto& item : items)
    {
        if (const auto* string_item = std::get_if<std::string>(&item))
            values.push_back(object{*string_item});
        else
            values.push_back(object{std::get<double>(item)});
    }

    brush_->op(content_operator::TJ, {object{array_object{std::move(values)}}});
    return *this;
}

text& text::show(std::initializer_list<text_show_item> items)
{
    return show(std::span<const text_show_item>{items.begin(), items.size()});
}

text& text::show_next_line(std::string_view encoded_text)
{
    ensure_active();
    brush_->op(content_operator::single_quote, {object{std::string{encoded_text}}});
    return *this;
}

text& text::show_next_line(double word_spacing, double character_spacing,
                           std::string_view encoded_text)
{
    ensure_active();
    brush_->op(content_operator::double_quote, {object{word_spacing}, object{character_spacing},
                                                object{std::string{encoded_text}}});
    return *this;
}

brush& text::end()
{
    ensure_active();

    brush_->op(content_operator::ET);
    brush_->in_text_ = false;
    ended_ = true;

    return *brush_;
}

void text::ensure_active() const
{
    if (ended_)
        throw logic_exception{"Text object has already been ended"};
}
} // namespace ripper::pdf::core::graphics
