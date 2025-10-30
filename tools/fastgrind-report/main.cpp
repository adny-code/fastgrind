#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>

#include "data.h"
#include "utils.h"
#include <map>
#include <set>

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
    label += strFormat(" [+%s, -%s]",
                       comma(std::to_string(node.mallocBytes())).c_str(),
                       comma(std::to_string(node.freeBytes())).c_str());

    if (node.childs().empty())
    {
        return Renderer([&, label] { return text(std::string("  ") + label); });
    }
    else
    {

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

template <typename T> class XYData
{
  public:
    void add(const T &x, const T &y)
    {
        _datas.emplace_back(std::make_pair(x, y));
    }

    std::pair<T, T> leftBottom() const
    {
        T x = std::numeric_limits<T>::max();
        T y = std::numeric_limits<T>::max();
        for (const auto &pp : _datas)
        {
            x = pp.first < x ? pp.first : x;
            y = pp.second < y ? pp.second : y;
        }

        return {x, y};
    }

    std::pair<T, T> size() const
    {
        T xl = std::numeric_limits<T>::max();
        T xr = std::numeric_limits<T>::min();
        T yb = std::numeric_limits<T>::max();
        T yt = std::numeric_limits<T>::min();
        for (const auto &pp : _datas)
        {
            xl = pp.first < xl ? pp.first : xl;
            yb = pp.second < yb ? pp.second : yb;
            xr = pp.first > xr ? pp.first : xr;
            yt = pp.second > yt ? pp.second : yt;
        }

        assert(yt - yb > 0);

        return {xr - xl, yt - yb};
    }

    void swap(std::vector<std::pair<T, T>> &data)
    {
        _datas.swap(data);
    }

    long trans(bool isX, long v) const
    {
        return isX ? v * scalerX - offsetX : v * scalerY - offsetY;
    }

    long rtrans(bool isX, long v) const
    {
        return isX ? double(v + offsetX) / scalerX : double(v + offsetY) / scalerY;
    }

    void trans(const std::pair<T, T> &nanchor, const std::pair<T, T> &nsize, std::vector<std::pair<T, T>> &out)
    {
        auto sanchor = leftBottom();
        auto ssize = size();
        scalerX = double(nsize.first) / ssize.first;
        scalerY = double(nsize.second) / ssize.second;
        offsetX = 1;
        offsetY = scalerY * sanchor.second;

        std::map<T, std::set<T>> tmp;
        for (const auto &pp : _datas)
        {
            T newX = pp.first * scalerX;
            T newY = pp.second * scalerY - offsetY;
            tmp[newX].insert(newY);
        }

        if (tmp.size() >= 2)
        {
            for (const auto &[x, l] : tmp)
            {
                if (l.size() >= 2)
                {
                    out.emplace_back(std::make_pair(x, *l.begin()));
                }

                out.emplace_back(std::make_pair(x, *l.rbegin()));
            }
        }
    }

    long getY(long x) const
    {
        for (const auto &pp : _datas)
        {
            if (pp.first >= x)
            {
                return pp.second;
            }
        }

        return -1;
    }

    long offsetX = 0;
    long offsetY = 0;
    double scalerX = 1.f;
    double scalerY = 1.f;

  protected:
    std::vector<std::pair<T, T>> _datas;
};

auto buildUiCurveView(const memData &data, const std::map<size_t, size_t> &xys)
{
    return Container::Vertical({Renderer([&] {
               auto terminal = Terminal::Size();
               int w = 2 * terminal.dimx - 2;
               int h = 4 * terminal.dimy - 4;
               auto c = Canvas(w, h);
               unsigned n = w + 1;
               std::string header = "Memory usage curve(time/KB)";
               c.DrawText(terminal.dimx - header.size(), 0, header);

               XYData<long> s2p;
               for (const auto &pp : xys)
                   s2p.add(pp.first, pp.second);

               std::vector<std::pair<long, long>> pxys;
               s2p.trans(std::pair<long, long>(5, 5), std::pair<long, long>(w - 10, h - 10), pxys);

               c.DrawText(0, 0, strFormat("cursor(%d, %d)", uiMouseX, uiMouseY));

               c.DrawPointLine(5, h - 5, 5, 5, Color::Black);
               c.DrawText(5, 5, "▲");

               c.DrawPointLine(5, h - 5, w - 5, h - 5, Color::Black);
               c.DrawText(w - 5, h - 5, "▶");

               c.DrawText(
                   uiMouseX + 2,
                   uiMouseY + 4,
                   strFormat("(%ld ,%ld)", s2p.rtrans(true, uiMouseX - 5), s2p.getY(s2p.rtrans(true, uiMouseX - 5))));

               c.DrawPointLine(uiMouseX, 10, uiMouseX, h - 10, Color::GrayLight);
               c.DrawPointLine(7,
                               h - 10 - s2p.trans(false, s2p.getY(s2p.rtrans(true, uiMouseX - 5))),
                               w - 7,
                               h - 10 - s2p.trans(false, s2p.getY(s2p.rtrans(true, uiMouseX - 5))),
                               Color::GrayLight);

#if 0
                // show border
               c.DrawPointLine(1, 1, w - 1, 1, Color::Black);
               c.DrawPointLine(1, 1, 1, h - 1, Color::Black);
               c.DrawPointLine(w - 1, 1, w - 1, h - 1, Color::Black);
               c.DrawPointLine(1, h - 1, w - 1, h - 1, Color::Black);
#endif

               auto y0 = s2p.trans(false, 0);
               printf("y0 %ld", y0);
               c.DrawPointLine(5, h - 10 - y0, w - 1, h - 10 - y0, Color::Blue1);

               if (pxys.size() >= 2)
               {
                   auto it = pxys.begin();
                   int x0 = it->first;
                   int y0 = it->second;
                   ++it;
                   do
                   {
                       int x1 = it->first;
                       int y1 = it->second;

                       c.DrawPointLine(x0 + 5, h - 10 - y0, x1 + 5, h - 10 - y1, Color::Black);
                       x0 = x1;
                       y0 = y1;
                   } while (++it != pxys.end());
               }

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

std::map<size_t, size_t> getPeakMemory(const memData &data)
{
    const auto &datas = data.datas;
    if (datas.empty())
    {
        return {};
    }

    // key is tick, second is cur used memory
    std::map<size_t, size_t> xy;
    auto it = datas.begin();
    long usedMemory = 0;
    long freeMemory = 0;
    do
    {
        const auto &tick = it->first;
        const auto &threads = it->second;

        usedMemory -= freeMemory;
        freeMemory = 0;

        for (const auto &[tid, node] : threads)
        {
            freeMemory += node.freeBytes();
            usedMemory += node.mallocBytes();
        }

        xy.emplace(tick, usedMemory);
    } while (++it != datas.end());
    return xy;
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

    auto memoryData = getPeakMemory(data);

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
        auto uiCurveView = buildUiCurveView(data, memoryData);

        mainWidget =
            Container::Vertical({Toggle(&uiTopBar, &uiTopBarSelected) | bgcolor(Color::Blue) | color(Color::White),
                                 Container::Tab({uiStackView, uiCurveView}, &uiTopBarSelected)});
    }

    screen.Loop(mainWidget);
    return 0;
}
