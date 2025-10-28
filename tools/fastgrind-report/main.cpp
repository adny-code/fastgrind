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

    std::shared_ptr<ComponentBase> mainWidget;
    if (data.datas.empty())
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
        mainWidget = Container::Vertical({Renderer([&] {
                                              return text(strFormat("fastgrind samples")) | bgcolor(Color::Blue) |
                                                     color(Color::White) | bold;
                                          }),
                                          buildHierUINode(data, root, root)});
    }

    screen.Loop(mainWidget);
    return 0;
}
