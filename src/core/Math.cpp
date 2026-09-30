#include "core/Math.h"
#include <DirectXMath.h>
namespace game {
Matrix transformMatrix(const Transform& t) {
    using namespace DirectX;
    XMFLOAT4X4 value;
    XMStoreFloat4x4(&value, XMMatrixScaling(t.scale.x,t.scale.y,t.scale.z)
        * XMMatrixRotationRollPitchYaw(t.rotation.x,t.rotation.y,t.rotation.z)
        * XMMatrixTranslation(t.position.x,t.position.y,t.position.z));
    Matrix result;
    std::copy_n(&value._11, 16, result.values.begin());
    return result;
}
Matrix multiply(const Matrix& local, const Matrix& parent) {
    using namespace DirectX;
    XMFLOAT4X4 a, b, c;
    std::copy_n(local.values.begin(), 16, &a._11);
    std::copy_n(parent.values.begin(), 16, &b._11);
    XMStoreFloat4x4(&c, XMLoadFloat4x4(&a) * XMLoadFloat4x4(&b));
    Matrix result; std::copy_n(&c._11, 16, result.values.begin()); return result;
}
}
