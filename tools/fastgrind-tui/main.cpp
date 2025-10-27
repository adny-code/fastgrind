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

    std::map<size_t, std::unordered_map<size_t, std::unordered_map<size_t, memFrame>>> frames;
    if (!loadData("fastgrind.json", frames))
    {
        printf("[error] fail to fastgrind.json\n");
        return 1;
    }

    std::shared_ptr<ComponentBase> mainWidget;
    if (frames.empty())
    {
        // no data
        mainWidget = Renderer([&]() {
            return vbox({paragraph("No sample in fastgrind.json"), paragraph("Press any key to exit")}) | border |
                   vcenter | center;
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
