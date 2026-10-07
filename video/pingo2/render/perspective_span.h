#ifndef PINGO_PERSPECTIVE_SPAN_H
#define PINGO_PERSPECTIVE_SPAN_H

#include <float.h>
#include <math.h>

typedef struct {
    float q, s, t;
    float dq, ds, dt;
    float boundary_u, boundary_v;
    float u, v, du, dv;
    int boundary_valid, valid, remaining;
#ifdef P2C_DIAGNOSTICS
    unsigned divisions;
#endif
} PingoPerspectiveSpan;

static inline int pingoPerspectiveRecover(PingoPerspectiveSpan *span,
                                          float q, float s, float t,
                                          float *u, float *v) {
    *u = *v = 0.0f;
    if (!isfinite(q) || !(q > 0.0f) || !isfinite(s) || !isfinite(t)) return 0;
#ifdef P2C_DIAGNOSTICS
    span->divisions += 2;
#else
    (void)span;
#endif
    *u = s / q;
    *v = t / q;
    return isfinite(*u) && isfinite(*v);
}

static inline void pingoPerspectiveBegin(PingoPerspectiveSpan *span,
                                         float q, float s, float t,
                                         float dq, float ds, float dt) {
    *span = (PingoPerspectiveSpan){.q=q, .s=s, .t=t, .dq=dq, .ds=ds, .dt=dt};
    span->boundary_valid = pingoPerspectiveRecover(
        span, q, s, t, &span->boundary_u, &span->boundary_v);
}

/* Emit 8 pixels using a carried boundary 8 pixels to the right. A final
 * 1..8-pixel block instead ends at its last covered pixel, never beyond it.
 * Caller must invoke once for every covered position, including depth rejects.
 * Invalid endpoints/steps return false so the caller can use its exact mapper;
 * boundary state still advances and can recover after a projective pole. */
static inline int pingoPerspectiveNext(PingoPerspectiveSpan *span,
                                       int pixels_remaining, float *u, float *v) {
    static const float inverse_steps[9] = {
        0.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f,
        0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f
    };
    if (pixels_remaining <= 0) { *u = *v = 0.0f; return 0; }
    if (!span->remaining) {
        int count = pixels_remaining > 8 ? 8 : pixels_remaining;
        int steps = pixels_remaining > 8 ? 8 : count - 1;
        float right_q = span->q + span->dq * steps;
        float right_s = span->s + span->ds * steps;
        float right_t = span->t + span->dt * steps;
        float right_u = span->boundary_u, right_v = span->boundary_v;
        int right_valid = span->boundary_valid;
        if (steps) right_valid = pingoPerspectiveRecover(
            span, right_q, right_s, right_t, &right_u, &right_v);
        span->u = span->boundary_u;
        span->v = span->boundary_v;
        span->du = (right_u - span->u) * inverse_steps[steps];
        span->dv = (right_v - span->v) * inverse_steps[steps];
        span->valid = span->boundary_valid && right_valid &&
            isfinite(span->du) && isfinite(span->dv);
        span->remaining = count;
        span->q = right_q; span->s = right_s; span->t = right_t;
        span->boundary_u = right_u; span->boundary_v = right_v;
        span->boundary_valid = right_valid;
    }
    *u = span->u; *v = span->v;
    span->u += span->du; span->v += span->dv;
    --span->remaining;
    return span->valid && isfinite(*u) && isfinite(*v);
}

/* Certify that the original per-pixel q and U/V are finite and q positive
 * throughout the triangle. Exact edge weights have one sign, sum to area,
 * and lie between zero and area. Margins bound their float dot products and
 * divisions away from overflow/underflow. This is an arithmetic safety guard,
 * not a span-length/texture/performance heuristic. Exceptional input retains
 * the original exact path. Rounded approximate endpoints have their own guard.
 */
static inline int pingoPerspectiveSafe(float inverse_area,
                                       const float q[3], const float s[3],
                                       const float t[3]) {
    float minimum_q = q[0];
    if (q[1] < minimum_q) minimum_q = q[1];
    if (q[2] < minimum_q) minimum_q = q[2];
    if (!(minimum_q >= FLT_MIN * 16.0f)) return 0;
    float dot_limit = (FLT_MAX * 0.125f) * fabsf(inverse_area);
    float uv_limit = (minimum_q < 1.0f ? minimum_q : 1.0f) * (FLT_MAX * 0.0625f);
    for (int i = 0; i < 3; ++i) {
        if (!isfinite(q[i]) || q[i] > dot_limit ||
            !isfinite(s[i]) || fabsf(s[i]) > dot_limit || fabsf(s[i]) > uv_limit ||
            !isfinite(t[i]) || fabsf(t[i]) > dot_limit || fabsf(t[i]) > uv_limit)
            return 0;
    }
    return 1;
}

#endif
