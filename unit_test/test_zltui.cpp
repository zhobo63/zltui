#include "unit_test.h"
#include "zltui.h"

void test_text(UnitReport& parent)
{

}

void test_richtext(UnitReport& parent)
{

}

void test_drawbuffer(UnitReport& parent)
{

}

void test_event(UnitReport& parent)
{

}

void test_editline(UnitReport& parent)
{

}

void test_win(UnitReport& parent)
{

}

void test_label(UnitReport& parent)
{

}

void test_button(UnitReport& parent)
{

}

void test_check(UnitReport& parent)
{

}

void test_combo(UnitReport& parent)
{

}

void test_slider(UnitReport& parent)
{

}

void test_edit(UnitReport& parent)
{

}

void test_labeledit(UnitReport& parent)
{

}

void test_richedit(UnitReport& parent)
{

}

void test_markdown(UnitReport& parent)
{

}

void test_styletext(UnitReport& parent)
{

}

void test_datepicker(UnitReport& parent)
{

}

void test_mgr(UnitReport& parent)
{

}

void test_zltui(UnitReport& parent)
{
    UnitReport unit("zltui");
    test_text(unit);
    test_richtext(unit);
    test_drawbuffer(unit);
    test_event(unit);
    test_editline(unit);
    test_win(unit);
    test_label(unit);
    test_button(unit);
    test_check(unit);
    test_combo(unit);
    test_slider(unit);
    test_edit(unit);
    test_labeledit(unit);
    test_richedit(unit);
    test_markdown(unit);
    test_styletext(unit);
    test_datepicker(unit);
    test_mgr(unit);

    parent.report.push_back(unit);
}
