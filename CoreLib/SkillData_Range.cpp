#include "pch.h"
#include "SkillData.h"

namespace Data {
    bool SkillRange::InRange(uint8_t dir, float cx, float cy, float mx, float my) const
    {
        float dx = mx - cx;
        float dy = my - cy;

        switch (type)
        {
        case SkillRangeType::Circle:
        {
            float r2 = range1 * range1;
            return (dx * dx + dy * dy) <= r2;
        }

        case SkillRangeType::Rectangle:
        {
            // range 1 길이, range2 폭
            float halfWidth = range2 * 0.5f;

            switch (dir)
            {
            case 0:
                return (dx >= -halfWidth && dx <= halfWidth && dy <= 0 && dy >= -range1);
            case 1: 
                return (dx >= -halfWidth && dx <= halfWidth && dy >= 0 && dy <= range1);
            case 2: 
                return (dy >= -halfWidth && dy <= halfWidth && dx <= 0 && dx >= -range1);
            case 3: 
                return (dy >= -halfWidth && dy <= halfWidth && dx >= 0 && dx <= range1);
            }
            return false;
        }
        case SkillRangeType::Boss1_1:
        {
            // range 1: 범위, range2: skill
            float r2 = range1 * range1;
            if ((dx * dx + dy * dy) > r2)
                return false;

            float angle = std::atan2(dy, dx) * 180.0f / 3.141592f;
            // atan (arctangent): vector의 각도 계산, 파이 값
            // 비싼 함수.. 
            if (angle < 0) angle += 360.0f;
            // UP, Down
            return (angle >= 210 && angle <= 330) || (angle >= 30 && angle <= 150);
        }
        case SkillRangeType::Boss1_2:
        {
            // range 1: 범위, range2: skill
            float r2 = range1 * range1;
            if ((dx * dx + dy * dy) > r2)
                return false;

            float angle = std::atan2(dy, dx) * 180.0f / 3.141592f;
            // atan (arctangent): vector의 각도 계산, 파이 값
            // 비싼 함수.. 
            if (angle < 0) angle += 360.0f;

           // RIGHT, Left
            return (angle >= 330 || angle <= 30) || (angle >= 150 && angle <= 210);
        }

        // -------------------------
        // 4) 보스2 : 직선 파동형 (range1: length, range2: width)
        //    dir 방향으로 뻗어가는 레이저/파동
        // -------------------------
        case SkillRangeType::Boss2:
        {
            float outerLen = range1;      // 바깥 사각형 길이
            float innerLen = range2;      // 빈 사각형 길이 (내부 구멍)

            float halfOuter = outerLen * 0.5f;
            float halfInner = innerLen * 0.5f;

            // 바깥 사각형 범위 벗어나면
            if (!(dx >= -halfOuter && dx <= halfOuter && dy <= halfOuter && dy >= -halfOuter))
                return false;
            // 안쪽 빈 공간에 존재하면
            if (dx >= -halfInner && dx <= halfInner && dy <= halfInner && dy >= -halfInner)
                return false;
            return true;
        }

        default:
            return false;
        }
    }
}