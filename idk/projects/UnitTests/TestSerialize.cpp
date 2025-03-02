#include "pch.h"
#include <core/GameObject.inl>
#include <common/Transform.h>
#include <scene/SceneManager.h>
#include <serialize/yaml.inl>
#include <res/ResourceHandle.inl>
#include "TestStructures.h"

using namespace idk;

TEST(Serialize, TestYaml)
{
	// test:
	//   a: x
	//   b: y
    yaml::node node = *yaml::load("test:\n  a: x\n  b: y");
    EXPECT_EQ(node["test"]["a"].as_scalar(), "x");
    EXPECT_EQ(node["test"]["b"].as_scalar(), "y");
    EXPECT_EQ(yaml::dump(node), "test: {a: x, b: y}\n");

	// - test: a:
	// - x: b
    yaml::node node2 = *yaml::load("- test: a:\n- x: b");
    EXPECT_EQ(node2[0]["test"].as_scalar(), "a:");
    EXPECT_EQ(node2[1]["x"].as_scalar(), "b");
    // - test: a:
    // - x: b
    EXPECT_EQ(yaml::dump(node2), "- test: a:\n- x: b\n");

	// -
	// - test:
	//   - x
	//   - y: hi
    //     z: bye
    //   - w
	yaml::node node3 = *yaml::load("-\n- test:\n  - x\n  - y: hi\n    z: bye\n  - w");
	EXPECT_TRUE(node3[0].is_null());
	EXPECT_EQ(node3[1]["test"][0].as_scalar(), "x");
	EXPECT_EQ(node3[1]["test"][1]["y"].as_scalar(), "hi");
    EXPECT_EQ(node3[1]["test"][1]["z"].as_scalar(), "bye");
    EXPECT_EQ(node3[1]["test"][2].as_scalar(), "w");
    // - 
    // - test: 
    //   - x
    //   - {y: hi, z: bye}
    //   - w
    EXPECT_EQ(yaml::dump(node3), "- \n- test: \n  - x\n  - {y: hi, z: bye}\n  - w\n");

	// - test: [x, y,{a: b} ]
	// - test2 : {b: [1,2,3]}
	yaml::node node4 = *yaml::load("- test: [x, y,{a: b} ]   \r\n- test2 : {b: [1,2,3]}");
    EXPECT_EQ(node4[0]["test"][0].as_scalar(), "x");
    EXPECT_EQ(node4[0]["test"][1].as_scalar(), "y");
    EXPECT_EQ(node4[0]["test"][2]["a"].as_scalar(), "b");
    EXPECT_EQ(node4[1]["test2"]["b"][0].get<int>(), 1);
    EXPECT_EQ(node4[1]["test2"]["b"][1].get<int>(), 2);
    EXPECT_EQ(node4[1]["test2"]["b"][2].get<int>(), 3);
    // - test:
    //   - x
    //   - y
    //   - a: b
    // - test2:
    //     b: [1, 2, 3]
    EXPECT_EQ(yaml::dump(node4), "- test: \n  - x\n  - y\n  - a: b\n- test2: \n    b: [1, 2, 3]\n");

    yaml::node node5 = *yaml::load("- !testtag '\"longassstring\"'");
	EXPECT_EQ(node5[0].as_scalar(), "\"longassstring\"");
	EXPECT_EQ(node5[0].tag(), "testtag");
    EXPECT_EQ(yaml::dump(node5), "[!testtag '\"longassstring\"']");
}

TEST(Serialize, TestSerializeBasic)
{
	EXPECT_TRUE(is_basic_serializable<float>::value);
	EXPECT_FALSE(is_basic_serializable<vec3>::value);
	EXPECT_TRUE(is_basic_serializable<Guid>::value);
	EXPECT_TRUE(is_basic_serializable<testserialize_enum>::value);

	testserialize_enum e = testserialize_enum::PI;
	EXPECT_STREQ(serialize_text(e).c_str(), "PI");

	vec3 v{ 1.0f, 2.0f, 3.0f };
	auto str = serialize_text(v);
	std::cout << str;
	EXPECT_STREQ(str.c_str(), "{x: 1, y: 2, z: 3}");

	serialize_this obj = {
		Guid{"e82bf459-faca-4c70-a8e9-dd35597575ef"},
		vec4{5.0f, 6.0f, 7.0f, 8.0f}
	};
	str = serialize_text(obj);
	std::cout << str;
	EXPECT_STREQ(str.c_str(), "f: 69\nguid: e82bf459-faca-4c70-a8e9-dd35597575ef\nvec: \n  w: 8\n  x: 5\n  y: 6\n  z: 7\n");

	// roundtrip
	auto obj2 = *parse_text<serialize_this>(str);
	EXPECT_EQ(obj.f, obj2.f);
	EXPECT_EQ(obj.guid, obj2.guid);
	EXPECT_EQ(obj.vec, obj2.vec);



	vector<string> yolo;
	yolo.emplace_back();
	parse_text(serialize_text(yolo), yolo);
    EXPECT_EQ(yolo.size(), 1);
    EXPECT_EQ(yolo[0], "");
}

TEST(Serialize, TestSerializeComplex)
{
	serialize_this_bs bs;
	bs.string_vec = { "ivan", "is", "a", "weeb" };
	bs.hashtable.emplace(Guid::Make(), "test0");
	bs.hashtable.emplace(Guid::Make(), "test1");

	auto str = serialize_text(bs);
	std::cout << str;
	
	// roundtrip
	serialize_this_bs bs2 = *parse_text<serialize_this_bs>(str);
	EXPECT_EQ(bs2.start, bs.start);
	EXPECT_EQ(bs2.mid, bs.mid);
	EXPECT_EQ(bs2.end, bs.end);
	EXPECT_EQ(bs2.string_vec[0], bs.string_vec[0]);
	EXPECT_EQ(bs2.string_vec[1], bs.string_vec[1]);
	EXPECT_EQ(bs2.string_vec[2], bs.string_vec[2]);
	EXPECT_EQ(bs2.string_vec[3], bs.string_vec[3]);
	EXPECT_EQ(bs2.hashtable[bs.hashtable.begin()->first], bs.hashtable.begin()->second);
	EXPECT_EQ(bs2.hashtable[(++bs.hashtable.begin())->first], (++bs.hashtable.begin())->second);
}

TEST(Serialize, TestSerializeUnknownAndParentAndVariant)
{
	serialass s;
	s.var = vec2{ 6.0f, 9.0f };
	s.guid = Guid::Make();
	s.vec = vec4{ 100, 200, 300, 400 };

	auto str = serialize_text(s);

	// roundtrip
	auto s2 = *parse_text<serialass>(str);

	EXPECT_EQ(s.f, s2.f);
	EXPECT_EQ(s.guid, s2.guid);
	EXPECT_EQ(s.vec, s2.vec);
	EXPECT_EQ(s.var, s2.var);
	EXPECT_EQ(s.x, s2.x);

	auto str2 = serialize_text(s2);

	EXPECT_EQ(str, str2);



	std::vector<reflect::dynamic> vec_of_dyns;
	vec_of_dyns.emplace_back(1);
	vec_of_dyns.emplace_back(1.0f);
	vec_of_dyns.emplace_back(testserialize_enum(testserialize_enum::TAU));
	auto s3 = serialize_text(vec_of_dyns);
    parse_text(s3, vec_of_dyns);

    EXPECT_EQ(vec_of_dyns[0].get<int>(), 1);
    EXPECT_EQ(vec_of_dyns[1].get<float>(), 1.0f);
    EXPECT_EQ(vec_of_dyns[2].get<testserialize_enum>(), testserialize_enum::TAU);
}

static string serialized_scene_0 = "";
static uint64_t transform_0_id = 0;
static uint64_t transform_1_id = 0;
static uint64_t parent_1_id = 0;

TEST(Serialize, TestSerializeScene)
{
    INIT_CORE();
    auto scene = Core::GetSystem<SceneManager>().GetActiveScene();

	auto o0 = scene->CreateGameObject();
	auto t0 = o0->GetComponent<Transform>();
	t0->position = vec3{ 1.0f, 2.0f, 3.0f };
	t0->scale = vec3{ 4.0f };
	t0->rotation = quat{ 5.0f, 6.0f, 7.0f, 8.0f };
	transform_0_id = t0.id;

	auto o1 = scene->CreateGameObject();
	auto t1 = o1->GetComponent<Transform>();
	t1->position = vec3{ 9.0f, 10.0f, 11.0f };
	t1->scale = vec3{ 12.0f };
	t1->rotation = quat{ 13.0f, 14.0f, 15.0f, 16.0f };
	transform_1_id = t1.id;
	t1->parent = o0;

	serialized_scene_0 = serialize_text(*scene);
	std::cout << serialized_scene_0;
}

TEST(Serialize, TestParseScene)
{
    INIT_CORE();
    auto scene = Core::GetSystem<SceneManager>().GetActiveScene();

	parse_text(serialized_scene_0, *scene);

	auto& o0 = *scene->begin();
	auto t0 = o0.GetComponent<Transform>();
	EXPECT_EQ(t0->position, vec3(1.0f, 2.0f, 3.0f));
	EXPECT_EQ(t0->scale, vec3{ 4.0f });
	EXPECT_EQ(t0->rotation, quat(5.0f, 6.0f, 7.0f, 8.0f));
	EXPECT_EQ(t0.id, transform_0_id);

	auto& o1 = *++scene->begin();
	auto t1 = o1.GetComponent<Transform>();
	EXPECT_EQ(t1->position, vec3(9.0f, 10.0f, 11.0f));
	EXPECT_EQ(t1->scale, vec3{ 12.0f });
	EXPECT_EQ(t1->rotation, quat(13.0f, 14.0f, 15.0f, 16.0f));
	EXPECT_EQ(t1.id, transform_1_id);
	EXPECT_EQ(o1.Parent(), o0.GetHandle());
}

TEST(Serialize, TestSerializeStructThatContainsDyn)
{
    structthatcontainsdyn x{ string("hello") };
    auto str = serialize_text(x);
    structthatcontainsdyn x2 = *parse_text<structthatcontainsdyn>(str);
    ASSERT_EQ(x.dyn.get<string>(), x2.dyn.get<string>());
}

TEST(Serialize, TestOnParseCallback)
{
    structonparse x;
    auto str = serialize_text(x);
    structonparse x2 = *parse_text<structonparse>(str);
    ASSERT_EQ(x2.x, 999);
}