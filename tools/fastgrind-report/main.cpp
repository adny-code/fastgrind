#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <map>
#include <set>

#include "data.h"
#include "utils.h"

using namespace ::ftxui;

// ==================== 全局变量 ====================
bool show = false;
std::vector<std::string> uiTopBar{"[stack view]", "[curve view]"};
int uiTopBarSelected = 0;
int uiMouseX = 0;
int uiMouseY = 0;
int uiCurveWidth = 10;
int uiCurveHeight = 10;

// ==================== 辅助组件 ====================

/**
 * @brief 创建可折叠的UI组件
 * @param label 折叠标签
 * @param child 子组件
 * @param show 控制显示/隐藏的引用
 */
Component uiCollapsible(ConstStringRef label, Component child, Ref<bool> show)
{
    class Impl : public ComponentBase
    {
      public:
        Impl(ConstStringRef label, Component child, Ref<bool> show) : show_(show)
        {
            CheckboxOption opt;
            opt.transform = [](EntryState s) {
                auto prefix = text(s.state ? "- " : "+ ");
                auto t = text(s.label);
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

      private:
        Ref<bool> show_;
    };

    return Make<Impl>(std::move(label), std::move(child), show);
}

// ==================== 内存树视图 ====================

/**
 * @brief 构建内存分配层次结构的UI节点
 * @param data 内存数据
 * @param root 根节点（用于计算百分比）
 * @param node 当前节点
 */
Component buildHierUINode(const memData &data, const memNode &root, const memNode &node)
{
    // 构建节点标签
    std::string label = strFormat("[%.2lf%% %.2lf%%]",
                                  100.f * node.mallocBytes() / root.mallocBytes(),
                                  100.f * node.freeBytes() / root.freeBytes());
    label += strFormat(" %s", data.getName(node.name()));
    label += strFormat(" [+%s, -%s]",
                       comma(std::to_string(node.mallocBytes())).c_str(),
                       comma(std::to_string(node.freeBytes())).c_str());

    // 如果是叶子节点
    if (node.childs().empty())
    {
        return Renderer([label] { return text(std::string("  ") + label); });
    }

    // 如果有子节点，构建可折叠的容器
    std::vector<const memNode *> subNodes;
    for (const auto &[nameId, subNode] : node.childs())
    {
        subNodes.push_back(&subNode);
    }

    // 按分配内存降序排序
    std::sort(subNodes.begin(), subNodes.end(), [](const memNode *a, const memNode *b) -> bool {
        if (a->mallocBytes() == b->mallocBytes())
        {
            return a->freeBytes() > b->freeBytes();
        }
        return a->mallocBytes() > b->mallocBytes();
    });

    // 构建子组件列表
    std::vector<Component> childs;
    for (const auto &subNode : subNodes)
    {
        childs.emplace_back(buildHierUINode(data, root, *subNode));
    }

    Component vlayout = Container::Vertical(childs);

    return uiCollapsible(label, Renderer(vlayout, [vlayout] { return hbox({text(" "), vlayout->Render()}); }), show);
}

// ==================== 无数据视图 ====================

/**
 * @brief 显示无数据的提示界面
 */
std::shared_ptr<ComponentBase> showNoData(ScreenInteractive &screen)
{
    auto widget = Renderer([&]() {
        return vbox({paragraph(strFormat("No sample in %s", __MEM_PATH_BINARY_RESULT)),
                     paragraph("Press any key to exit")}) |
               border | vcenter | center;
    });

    widget |= CatchEvent([&](Event event) -> bool {
        // 按任意键退出
        if (event.is_character())
        {
            screen.Exit();
            return true;
        }
        return false;
    });

    return widget;
}

// ==================== 曲线图数据类 ====================

/**
 * @brief 用于存储和处理XY坐标数据的模板类
 */
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

    /**
     * @brief 坐标转换：将原始数据转换到指定范围的画布坐标
     */
    void trans(const std::pair<T, T> &nanchor, const std::pair<T, T> &nsize, std::vector<std::pair<T, T>> &out)
    {
        auto sanchor = leftBottom();
        auto ssize = size();

        scalerX = double(nsize.first) / ssize.first;
        scalerY = double(nsize.second) / ssize.second;
        offsetX = 1;
        offsetY = scalerY * sanchor.second;

        // 对相同X值的Y值进行聚合
        std::map<T, std::set<T>> tmp;
        for (const auto &pp : _datas)
        {
            T newX = pp.first * scalerX;
            T newY = pp.second * scalerY - offsetY;
            tmp[newX].insert(newY);
        }

        // 生成曲线点（取每个X值的最小和最大Y值）
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

    // 转换参数
    long offsetX = 0;
    long offsetY = 0;
    double scalerX = 1.f;
    double scalerY = 1.f;

  protected:
    std::vector<std::pair<T, T>> _datas;
};

// ==================== 曲线图视图 ====================

/**
 * @brief 构建内存使用曲线图视图
 */
auto buildUiCurveView(const memData &data, const std::map<size_t, size_t> &xys)
{
    return Container::Vertical({Renderer([&] {
               auto terminal = Terminal::Size();
               int w = 2 * terminal.dimx - 2;
               int h = 4 * terminal.dimy - 4;
               auto c = Canvas(w, h);

               // 绘制标题
               std::string header = "Memory usage curve(time/KB)";
               c.DrawText(terminal.dimx - header.size(), 0, header);

               // 准备数据
               XYData<long> s2p;
               for (const auto &pp : xys)
               {
                   s2p.add(pp.first, pp.second);
               }

               // 坐标转换
               std::vector<std::pair<long, long>> pxys;
               s2p.trans(std::pair<long, long>(5, 5), std::pair<long, long>(w - 10, h - 10), pxys);

               // 绘制坐标轴
               c.DrawPointLine(5, h - 5, 5, 5, Color::Black);
               c.DrawText(5, 5, "▲"); // Y轴箭头

               c.DrawPointLine(5, h - 5, w - 5, h - 5, Color::Black);
               c.DrawText(w - 5, h - 5, "▶"); // X轴箭头

               // 绘制光标和提示
               c.DrawText(0, 0, strFormat("cursor(%d, %d)", uiMouseX, uiMouseY));
               c.DrawText(
                   uiMouseX + 2,
                   uiMouseY + 4,
                   strFormat("(%ld ,%ld)", s2p.rtrans(true, uiMouseX - 5), s2p.getY(s2p.rtrans(true, uiMouseX - 5))));

               // 绘制十字准线
               c.DrawPointLine(uiMouseX, 10, uiMouseX, h - 10, Color::GrayLight);
               c.DrawPointLine(7,
                               h - 10 - s2p.trans(false, s2p.getY(s2p.rtrans(true, uiMouseX - 5))),
                               w - 7,
                               h - 10 - s2p.trans(false, s2p.getY(s2p.rtrans(true, uiMouseX - 5))),
                               Color::GrayLight);

               // 绘制零线
               auto y0 = s2p.trans(false, 0);
               c.DrawPointLine(5, h - 10 - y0, w - 1, h - 10 - y0, Color::Blue1);

               // 绘制曲线
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
               // 鼠标事件处理
               if (e.is_mouse())
               {
                   uiMouseX = (e.mouse().x - 1) * 2;
                   uiMouseY = (e.mouse().y - 1) * 4;
               }
               return false;
           });
}

// ==================== 数据处理函数 ====================

/**
 * @brief 获取峰值内存使用数据
 * @return map<时间点, 已使用内存>
 */
std::map<size_t, size_t> getPeakMemory(const memData &data)
{
    const auto &datas = data.datas;
    if (datas.empty())
    {
        return {};
    }

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

// ==================== 主函数 ====================

int main()
{
    using namespace ftxui;
    auto screen = ScreenInteractive::Fullscreen();

    // 加载数据
    memData data;
    if (!loadData(__MEM_PATH_BINARY_RESULT, data))
    {
        printf("[error] fail to load fastgrind.json\n");
        return 1;
    }
    else
    {
        printf("[info] loaded data from '%s'\n", __MEM_PATH_BINARY_RESULT);
    }

    // 构建内存树
    memNode root("root");
    for (const auto &[tick, ppp] : data.datas)
    {
        for (const auto &[tid, node] : ppp)
        {
            root.add(node, false);
        }
    }

    auto terminal = Terminal::Size();
    printf("[info] terminal size %d x %d\n", terminal.dimx, terminal.dimy);

    // 获取内存曲线数据
    auto memoryData = getPeakMemory(data);

    // 构建主界面
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

    // 运行界面
    screen.Loop(mainWidget);
    return 0;
}
