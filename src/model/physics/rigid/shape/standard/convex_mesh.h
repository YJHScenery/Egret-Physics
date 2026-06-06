//
// Created by jehor on 2026/6/6.
//

#ifndef EGRET_PHYSICS_CONVEX_MESH_H
#define EGRET_PHYSICS_CONVEX_MESH_H
#include "shape_base.h"
#include "geometry_utils.h"

namespace egret
{
    /**
     * @brief       闭合凸体 Mesh 形，表示任意凸体刚体的碰撞形状。
     * @details     ConvexMesh 继承自 ShapeBase，表示一个通过 TessellatedMesh 所构建的 Mesh 网格。
     *              用于物理碰撞检测和转动惯量计算。
     *              此形状不具有良好的性质，其碰撞检测、转动惯量矩阵、AABB 盒子都必须通过穷举计算。
     *              构造时，会对模型执行封闭与凸体检查，如果不符合要求，则拒绝构造
     *
     * @invariant   typeId() 返回 ShapeType::ConvexMesh
     * @invariant   质心通过积分获得，为了方便计算，会通过对 Mesh 信息的转化，将质心强制推送至坐标系原点
     * @remark      形状的质心将会被严格计算移动到零点，此时所有的位置信息将会交由 Transform 管理
     * @see         ShapeBase, Transform, AABB, TessellatedMesh
     */
    class ConvexMesh: public ShapeBase
    {
    public:
        explicit ConvexMesh(const TessellatedMesh& mesh);

        ShapeType getType() const override;

    private:

    };
} // egret

#endif //EGRET_PHYSICS_CONVEX_MESH_H
