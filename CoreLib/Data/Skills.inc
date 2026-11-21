// Server-owned canonical data. Export client snapshots with tools/Export-ServerContent.ps1.
SkillData{
    0, SkillFlags::None,
    0, 20,
    {
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 6, SkillActionType::Hit, {SkillRangeType::Circle, 2.0f, 0.0f}, 30 },
    }
},
SkillData{
    1, SkillFlags::AreaOfEffect,
    100, 120,
    {
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 16, SkillActionType::Hit, {SkillRangeType::Circle, 8.0f, 0.0f}, 120 },
    }
},
SkillData{
    2, SkillFlags::AreaOfEffect,
    160, 80,
    {
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 10, SkillActionType::Hit, {SkillRangeType::Rectangle, 12.0f, 3.0f}, 180 },
    }
},
SkillData{
    3, SkillFlags::AreaOfEffect,
    0, 0,
    {
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss1_1, 10.0f, 30.0f}, 100 },
        SkillPhase{ 10, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss1_2, 10.0f, 30.0f}, 100 },
    }
},
SkillData{
    4, SkillFlags::AreaOfEffect,
    0, 0,
    {
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss2, 5.0f, 0.0f}, 100 },
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss2, 10.0f, 5.0f}, 100 },
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss2, 15.0f, 10.0f}, 100 },
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss2, 20.0f, 15.0f}, 100 },
        SkillPhase{ 0, SkillActionType::Charging, {SkillRangeType::None, 0.0f, 0.0f}, 0 },
        SkillPhase{ 20, SkillActionType::Hit, {SkillRangeType::Boss2, 25.0f, 20.0f}, 100 },
    }
},
