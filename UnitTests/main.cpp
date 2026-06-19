#include <gtest/gtest.h>
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}


/*
EXPECT(계속) / ASSERT(중단)
EXPECT_EQ(a, b);    // a == b
EXPECT_NE(a, b);    // a != b
EXPECT_TRUE(x);     // x == true
EXPECT_FALSE(x);    // x == false
EXPECT_LT(a, b);    // a <  b   (Less Than)
EXPECT_LE(a, b);    // a <= b   (Less Equal)
EXPECT_GT(a, b);    // a >  b   (Greater Than)
EXPECT_GE(a, b);    // a >= b   (Greater Equal)

포인터 / null
EXPECT_EQ(ptr, nullptr);
EXPECT_NE(ptr, nullptr);
ASSERT_NE(ptr, nullptr);

부동소수점 (== 비교 안 됨, 오차 때문)
EXPECT_FLOAT_EQ(a, b);        // float 근사 비교
EXPECT_DOUBLE_EQ(a, b);       // double
EXPECT_NEAR(a, b, 0.001);     // 허용 오차 직접 지정

예외 검증
EXPECT_THROW(expr, std::out_of_range);  // 이 예외 던지나
EXPECT_NO_THROW(expr);                  // 예외 안 던지나
EXPECT_ANY_THROW(expr);                 // 뭐든 던지나

실패 메시지 붙이기 (<< 스트림)
EXPECT_EQ(q.size(), 1) << "push 후 size는 1이어야 함, 실제: " << q.size();

테스트 정의
TEST(SuiteName, TestName) {
    // 본문
}

Fixture — 공통 셋업/정리
class ObjectPoolTest : public ::testing::Test { ... };
TEST_F(ObjectPoolTest, ...)   // ← 클래스 이름과 일치 필수
*/