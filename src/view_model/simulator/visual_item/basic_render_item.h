//
// Created by jehor on 2026/6/6.
//

#ifndef EGRET_PHYSICS_BASIC_RENDER_ITEM_H
#define EGRET_PHYSICS_BASIC_RENDER_ITEM_H

template <typename StrType>
struct BasicRenderItem
{
    /** 实体 ID。 */
    std::uint64_t id{0};

    /** 渲染类型，例如 sphere、box、particle。 */
    std::uint32_t kind{0};

    /** 实体名称 */
    StrType name{};

    /** 实体颜色 */
    StrType color{};

    /** 必要的注解说明 */
    StrType label{};
};


#endif //EGRET_PHYSICS_BASIC_RENDER_ITEM_H
