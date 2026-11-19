// Server-owned canonical data. Export client snapshots with tools/Export-ServerContent.ps1.
SkillData{
    0, 0,
    1, 20,
    {
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 6, 1, {SkillRangeType::Circle, 2.0f, 0.0f}, 5 },
    }
},
SkillData{
    1, 2,
    20, 120,
    {
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 16, 1, {SkillRangeType::Circle, 8.0f, 0.0f}, 15 },
    }
},
SkillData{
    2, 2,
    20, 80,
    {
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 16, 1, {SkillRangeType::Rectangle, 5.0f, 2.5f}, 12 },
    }
},
SkillData{
    3, 2,
    0, 0,
    {
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss1_1, 10.0f, 30.0f}, 100 },
        SkillPhase{ 10, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss1_2, 10.0f, 30.0f}, 100 },
    }
},
SkillData{
    4, 2,
    0, 0,
    {
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss2, 5.0f, 0.0f}, 100 },
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss2, 10.0f, 5.0f}, 100 },
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss2, 15.0f, 10.0f}, 100 },
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss2, 20.0f, 15.0f}, 100 },
        SkillPhase{ 0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, 1, {SkillRangeType::Boss2, 25.0f, 20.0f}, 100 },
    }
},
// Forest sweep, outpost cleave and altar burst reuse the ordinary two-phase attack flow.
SkillData{5, 2, 0, 0, {
    SkillPhase{0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0},
    SkillPhase{14, 1, {SkillRangeType::Circle, 3.5f, 0.0f}, 10},
}},
SkillData{6, 2, 0, 0, {
    SkillPhase{0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0},
    SkillPhase{20, 1, {SkillRangeType::Rectangle, 4.5f, 5.0f}, 11},
}},
SkillData{7, 2, 0, 0, {
    SkillPhase{0, 0, {SkillRangeType::None, 0.0f, 0.0f}, 0},
    SkillPhase{24, 1, {SkillRangeType::Circle, 6.0f, 0.0f}, 12},
}},
