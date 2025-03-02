#include "pch.h" // gtest.h
#include <ds/small_string.h>

TEST(SmallString, TestSmallString)
{
	EXPECT_EQ(sizeof(idk::small_string<char>), 32);

	idk::small_string<char> str;
	EXPECT_EQ(str.size(), 0);
	EXPECT_STREQ(str.c_str(), "");

	str.push_back('a');
	EXPECT_EQ(str.size(), 1);
	EXPECT_EQ(str.capacity(), 31);
	EXPECT_STREQ(str.c_str(), "a");

	str.append("ivan is a weeb");
	EXPECT_EQ(str.size(), 15);
	EXPECT_EQ(str.capacity(), 31);
	EXPECT_STREQ(str.c_str(), "aivan is a weeb");

	str += str;
	EXPECT_EQ(str.size(), 30);
	EXPECT_EQ(str.capacity(), 31);
	EXPECT_STREQ(str.c_str(), "aivan is a weebaivan is a weeb");

	str += 'x';
	EXPECT_EQ(str.size(), 31);
	EXPECT_EQ(str.capacity(), 31);
	EXPECT_STREQ(str.c_str(), "aivan is a weebaivan is a weebx");

	str += "x";
	EXPECT_EQ(str.size(), 32);
	EXPECT_EQ(str.capacity(), 63);
	EXPECT_STREQ(str.c_str(), "aivan is a weebaivan is a weebxx");

	str.erase(0, 1);
	EXPECT_EQ(str.size(), 31);
	EXPECT_EQ(str.capacity(), 63);
	EXPECT_STREQ(str.c_str(), "ivan is a weebaivan is a weebxx");

	str.resize(14);
	EXPECT_EQ(str.size(), 14);
	EXPECT_EQ(str.capacity(), 63);
	EXPECT_STREQ(str.c_str(), "ivan is a weeb");

	auto i = str.find("weeb");
	EXPECT_EQ(i, 10);

	i = str.rfind("weeb");
	EXPECT_EQ(i, 10);

	i = str.find("weeeeb", 0, 2);
	EXPECT_EQ(i, 10);

	i = str.rfind("ivan");
	EXPECT_EQ(i, 0);

	EXPECT_EQ(str[4], ' ');
	EXPECT_EQ(str, idk::small_string<char>(str));
	EXPECT_NE(str, str + str);
	EXPECT_LT(str, "j");
	EXPECT_LT("h", str);

	str.clear();
	EXPECT_EQ(str.size(), 0);
	EXPECT_EQ(str.capacity(), 63);
}