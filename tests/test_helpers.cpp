#include "Test.h"
#include "engine/ChannelHelper.h"
#include "engine/StringHelper.h"

using namespace eqapo;

TEST(string_trim_split) {
    CHECK_EQ(StringHelper::trim("  a b \t"), std::string("a b"));
    CHECK_EQ(StringHelper::trim("   "), std::string(""));
    std::vector<std::string> parts = StringHelper::split("a=b+ +c", '+');
    CHECK_EQ(parts, (std::vector<std::string>{"a=b", " ", "c"}));
    parts = StringHelper::split("L=0.5*R", '*');
    CHECK_EQ(parts, (std::vector<std::string>{"L=0.5", "R"}));
    parts = StringHelper::split("a,,b", ',', false);
    CHECK_EQ(parts, (std::vector<std::string>{"a", "", "b"}));
    CHECK_EQ(StringHelper::replaceCharacters("1,5 dB", ",", "."), std::string("1.5 dB"));
    CHECK_EQ(StringHelper::toLowerCase("MiXeD"), std::string("mixed"));
    CHECK(StringHelper::startsWith("Filter 1", "Filter"));
    CHECK(!StringHelper::startsWith("Fil", "Filter"));
    double v = 0;
    CHECK(StringHelper::parseDouble(" -6.5 dB", v));
    CHECK_NEAR(v, -6.5, 1e-12);
    CHECK(!StringHelper::parseDouble("dB", v));
}

TEST(channel_names_by_count) {
    CHECK_EQ(ChannelHelper::getChannelNames(1), (Channels{"C"}));
    CHECK_EQ(ChannelHelper::getChannelNames(2), (Channels{"L", "R"}));
    CHECK_EQ(ChannelHelper::getChannelNames(4), (Channels{"L", "R", "RL", "RR"}));
    CHECK_EQ(ChannelHelper::getChannelNames(6), (Channels{"L", "R", "C", "LFE", "SL", "SR"}));
    CHECK_EQ(ChannelHelper::getChannelNames(8), (Channels{"L", "R", "C", "LFE", "RL", "RR", "SL", "SR"}));
    CHECK_EQ(ChannelHelper::getChannelNames(3), (Channels{"1", "2", "3"}));
    CHECK_EQ(ChannelHelper::getChannelNames(0), (Channels{}));
}

TEST(channel_index_lookup) {
    Channels stereo = ChannelHelper::getChannelNames(2);
    CHECK_EQ(ChannelHelper::getChannelIndex("L", stereo), 0);
    CHECK_EQ(ChannelHelper::getChannelIndex("R", stereo), 1);
    CHECK_EQ(ChannelHelper::getChannelIndex("2", stereo), 1);
    CHECK_EQ(ChannelHelper::getChannelIndex("3", stereo), -1);
    CHECK_EQ(ChannelHelper::getChannelIndex("0", stereo), -1);
    CHECK_EQ(ChannelHelper::getChannelIndex("C", stereo), -1);
    CHECK_EQ(ChannelHelper::getChannelIndex("C", stereo, true), -1);

    Channels quad = ChannelHelper::getChannelNames(4);
    CHECK_EQ(ChannelHelper::getChannelIndex("SL", quad), 2);  // alias to RL
    CHECK_EQ(ChannelHelper::getChannelIndex("SR", quad), 3);
    Channels six = ChannelHelper::getChannelNames(6);
    CHECK_EQ(ChannelHelper::getChannelIndex("RL", six), 4);   // alias to SL
    CHECK_EQ(ChannelHelper::getChannelIndex("SUB", six), 3);  // old name for LFE
}
