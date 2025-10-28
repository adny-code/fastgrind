#include <ftxui/component/component.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <cstdio>
#include <iostream>

#include <map>

#include "data.h"
#include "utils.h"

int main()
{
    using namespace ftxui;
    auto screen = ScreenInteractive::Fullscreen();

    memSerializerMap<const char *, memSerializeString> names;
    memSerializerMap<size_t, memSerializerMap<size_t, memNode>> datas;
    if (!loadData(__MEM_PATH_BINARY_RESULT, names, datas))
    {
        printf("[error] fail to fastgrind.json\n");
        return 1;
    }
    else
    {
        printf("[info] loaded data from '%s'\n", __MEM_PATH_BINARY_RESULT);
    }

    std::shared_ptr<ComponentBase> mainWidget;
    if (datas.empty())
    {
        // no data
        mainWidget = Renderer([&]() {
            return vbox({paragraph(strFormat("No sample in %s", __MEM_PATH_BINARY_RESULT)),
                         paragraph("Press any key to exit")}) |
                   border | vcenter | center;
        });

        mainWidget |= CatchEvent([&](Event event) -> bool {
            // press any key or mouse to exit
            if (event.is_character())
            {
                screen.Exit();
                return true;
            }
            else
            {
                return false;
            }
        });
    }
    else
    {
        mainWidget = Container::Vertical(
            {Renderer([&] { return text(strFormat("fastgrind samples")) | bgcolor(Color::Blue); })});
    }

    screen.Loop(mainWidget);
    return 0;
}
