#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>

#include <map>

#include "data.h"
#include "utils.h"

using namespace ::ftxui;

bool show = false;
std::vector<std::string> uiTopBar{"[stack view]", "[curve view]"};
int uiTopBarSelected = 0;
int uiMouseX = 0;
int uiMouseY = 0;

int uiCurveWidth = 10;
int uiCurveHeight = 10;

Component uiCollapsible(ConstStringRef label, Component child, Ref<bool> show)
{
    class Impl : public ComponentBase
    {
      public:
        Impl(ConstStringRef label, Component child, Ref<bool> show) : show_(show)
        {
            CheckboxOption opt;
            opt.transform = [](EntryState s) {             // NOLINT
                auto prefix = text(s.state ? "- " : "+ "); // NOLINT
                auto t = text(s.label);
                if (s.active)
                {
                    // t |= bold;
                }
                if (s.focused)
                {
                    t |= inverted;
                }
                return hbox({prefix, t});
            };
            Add(Container::Vertical({
                Checkbox(std::move(label), show_.operator->(), opt),
                Maybe(std::move(child), show_.operator->()),
            }));
        }
        Ref<bool> show_;
    };

    return Make<Impl>(std::move(label), std::move(child), show);
}

Component buildHierUINode(const memData &data, const memNode &root, const memNode &node)
{
    std::string label = strFormat("[%.2lf%% %.2lf%%]",
                                  100.f * node.mallocBytes() / root.mallocBytes(),
                                  100.f * node.freeBytes() / root.freeBytes());
    label += strFormat(" %s", data.getName(node.name()));
    label += strFormat(" [+%lu, -%lu]", node.mallocBytes(), node.freeBytes());

    std::vector<const memNode *> subNodes;
    for (const auto &[nameId, subNode] : node.childs())
        subNodes.push_back(&subNode);

    std::sort(subNodes.begin(), subNodes.end(), [](const memNode *a, const memNode *b) -> bool {
        if (a->mallocBytes() == b->mallocBytes())
        {
            return a->freeBytes() > b->freeBytes();
        }
        else
        {
            return a->mallocBytes() > b->mallocBytes();
        }
    });

    std::vector<Component> childs;
    for (const auto &subNode : subNodes)
    {
        childs.emplace_back(buildHierUINode(data, root, *subNode));
    }

    Component vlayout = Container::Vertical(childs);

    return uiCollapsible(label,
                         Renderer(vlayout,
                                  [vlayout] {
                                      return hbox({
                                          text(" "),
                                          vlayout->Render(),
                                      });
                                  }),
                         show);
}

std::shared_ptr<ComponentBase> showNoData(ScreenInteractive &screen)
{
    std::shared_ptr<ComponentBase> widget;
    widget = Renderer([&]() {
        return vbox({paragraph(strFormat("No sample in %s", __MEM_PATH_BINARY_RESULT)),
                     paragraph("Press any key to exit")}) |
               border | vcenter | center;
    });

    widget |= CatchEvent([&](Event event) -> bool {
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

    return widget;
}

auto buildUiCurveView(const memData &data)
{
    return Container::Vertical({Renderer([&] {
               auto terminal = Terminal::Size();
               int w = 2 * terminal.dimx - 2;
               int h = 4 * terminal.dimy - 4;
               auto c = Canvas(w, h);
               unsigned n = w + 1;
               std::string header = "Memory usage curve(time/KB)";
               c.DrawText(terminal.dimx - header.size(), 0, header);

               c.DrawText(0, 0, strFormat("cursor(%d, %d)", uiMouseX, uiMouseY));

               c.DrawPointLine(5, h - 5, 5, 5, Color::Black);
               c.DrawText(5, 5, "▲");

               c.DrawPointLine(5, h - 5, w - 5, h - 5, Color::Black);
               c.DrawText(w - 5, h - 5, "▶");

               c.DrawPointLine(uiMouseX, 10, uiMouseX, h - 10, Color::GrayLight);
               c.DrawPointLine(7, uiMouseY, w - 7, uiMouseY, Color::GrayLight);
               c.DrawText(uiMouseX + 2, uiMouseY + 4, strFormat("(%d ,%d)", uiMouseX, uiMouseY));

#if 0
               c.DrawPointLine(1, 1, w - 1, 1, Color::Black);
               c.DrawPointLine(1, 1, 1, h - 1, Color::Black);
               c.DrawPointLine(w - 1, 1, w - 1, h - 1, Color::Black);
               c.DrawPointLine(1, h - 1, w - 1, h - 1, Color::Black);
#endif
        //    for (int y = 25; y < h; y += 25)
        //    {
        //        int margin = 0;
        //        c.DrawPointLine(margin, y, w - margin, y, Color::GrayLight);
        //    }

#if 1
               std::vector<int> ys(n);
               for (int x = 0; x < n; x++)
               {
                   float dx = float(x - /*uiMouseX*/ 0);
                   float dy = 50.f;
                   ys[x] = int(dy + 20 * cos(dx * 0.14) + 10 * sin(dx * 0.42));
               }
               for (int x = 1; x < n - 1; x++)
               {
                   c.DrawPointLine(x, ys[x], x + 1, ys[x + 1]);
               }

#endif

               return canvas(std::move(c));
           })}) |
           CatchEvent([&](Event e) {
               if (e.is_mouse())
               {
                   uiMouseX = (e.mouse().x - 1) * 2;
                   uiMouseY = (e.mouse().y - 1) * 4;
               }
               return false;
           });
}

int main()
{
    using namespace ftxui;
    auto screen = ScreenInteractive::Fullscreen();

    memData data;
    if (!loadData(__MEM_PATH_BINARY_RESULT, data))
    {
        printf("[error] fail to fastgrind.json\n");
        return 1;
    }
    else
    {
        printf("[info] loaded data from '%s'\n", __MEM_PATH_BINARY_RESULT);
    }

    memNode root("root");
    for (const auto &[tick, ppp] : data.datas)
    {
        for (const auto &[tid, node] : ppp)
        {
            root.add(node, false);
        }
    }
    auto terminal = Terminal::Size();
    printf("[info] size %d %d\n", terminal.dimx, terminal.dimy);

    std::shared_ptr<ComponentBase> mainWidget;
    if (data.datas.empty())
    {
        mainWidget = showNoData(screen);
    }
    else
    {
        auto uiStackView = Container::Vertical({Renderer([&]() {
                                                    return text(strFormat("fastgrind samples")) | bgcolor(Color::Blue) |
                                                           color(Color::White) | bold;
                                                }),
                                                buildHierUINode(data, root, root) | vscroll_indicator | frame});
        auto uiCurveView = buildUiCurveView(data);

        mainWidget =
            Container::Vertical({Toggle(&uiTopBar, &uiTopBarSelected) | bgcolor(Color::Blue) | color(Color::White),
                                 Container::Tab({uiStackView, uiCurveView}, &uiTopBarSelected)});
    }

    screen.Loop(mainWidget);
    return 0;
}
