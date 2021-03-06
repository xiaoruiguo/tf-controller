/*
 * Copyright (c) 2013 Juniper Networks, Inc. All rights reserved.
 */

#include "base/logging.h"
#include "base/address.h"
#include "testing/gunit.h"

#include "filter/acl_entry.h"
#include "filter/acl_entry_spec.h"
#include "filter/packet_header.h"
#include "filter/traffic_action.h"
#include "filter/acl.h"
#include "oper/mirror_table.h"

void RouterIdDepInit(Agent *agent) {
}

namespace {
class AclEntryTest : public ::testing::Test {
protected:
};

TEST_F(AclEntryTest, Basic) {
    AclEntrySpec ae_spec1;
    ae_spec1.id = 1;
    ae_spec1.BuildAddressInfo("1.1.1.1", 32, &ae_spec1.src_ip_list);
    ae_spec1.src_addr_type = AddressMatch::IP_ADDR;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec port;
    port.min = 10;
    port.max = 100;
    ae_spec1.dst_port.push_back(port);
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::PASS;
    //ae_spec1.permit_deny = true;
    ae_spec1.action_l.push_back(action);

    AclEntry *entry1 = new AclEntry();
    entry1->PopulateAclEntry(ae_spec1);

    PacketHeader *packet1 = new PacketHeader();
    packet1->src_ip = Ip4Address(0x01010102);
    packet1->dst_ip = Ip4Address(0xFFFFFF00);
    packet1->protocol = 10;
    packet1->dst_port = 99;

    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(1U, al.size());
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    // EXPECT_TRUE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 11;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 10;
    //Destination port would be ignored, since protocol is neither TCP or UDP
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(1U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 9;
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 9;
    packet1->dst_port = 100;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    delete packet1;
    delete entry1;
}

TEST_F(AclEntryTest, Ipv6_address_match) {
    boost::system::error_code ec;
    AclEntrySpec ae_spec1;
    ae_spec1.id = 1;
    ae_spec1.BuildAddressInfo("fd11::3", 128, &ae_spec1.src_ip_list);
    ae_spec1.src_addr_type = AddressMatch::IP_ADDR;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec port;
    port.min = 10;
    port.max = 100;
    ae_spec1.dst_port.push_back(port);
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::PASS;
    ae_spec1.action_l.push_back(action);

    AclEntry *entry1 = new AclEntry();
    entry1->PopulateAclEntry(ae_spec1);

    PacketHeader *packet1 = new PacketHeader();
    packet1->src_ip = Ip6Address::from_string("fd11::4", ec);
    packet1->dst_ip = Ip6Address::from_string("fd11::10", ec);
    packet1->protocol = 10;
    packet1->dst_port = 99;

    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1->PacketMatch(*packet1, &info);
    //Verify that source address match fails when src address in the packet is
    //different
    EXPECT_EQ(0U, al.size());

    packet1->src_ip = Ip6Address::from_string("fd11::3", ec);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);

    //Verify that source address match fails when src address in the packet is
    //same
    EXPECT_EQ(1U, al.size());
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(SimpleAction::PASS,
              static_cast<SimpleAction *>(*ial.operator->())->action());

    //cleanup
    delete packet1;
    delete entry1;
}

TEST_F(AclEntryTest, SubnetAddress) {
    AclEntrySpec ae_spec1;
    ae_spec1.id = 1;
    ae_spec1.BuildAddressInfo("1.1.1.0", 24, &ae_spec1.src_ip_list);
    ae_spec1.src_addr_type = AddressMatch::IP_ADDR;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec port;
    port.min = 10;
    port.max = 100;
    ae_spec1.dst_port.push_back(port);
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::PASS;
    ae_spec1.action_l.push_back(action);
    //ae_spec1.permit_deny = true;

    AclEntry *entry1 = new AclEntry();
    PacketHeader *packet1 = new PacketHeader();
    packet1->src_ip = Ip4Address(0x01010102);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    entry1->PopulateAclEntry(ae_spec1);
    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1->PacketMatch(*packet1, &info);
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(TrafficAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010110);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 11;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010102);
    packet1->protocol = 10;
    //Destination port would be ignored, since protocol is neither TCP or UDP
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(1U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010103);
    packet1->protocol = 9;
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010104);
    packet1->protocol = 9;
    packet1->dst_port = 100;
    al = entry1->PacketMatch(*packet1, &info);
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    delete packet1;
    delete entry1;
}


TEST_F(AclEntryTest, BasicAccept) {
    AclEntrySpec ae_spec1;

    ae_spec1.id = 1;
    ae_spec1.BuildAddressInfo("1.1.1.0", 24, &ae_spec1.src_ip_list);
    ae_spec1.src_addr_type = AddressMatch::IP_ADDR;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec port;
    port.min = 10;
    port.max = 100;
    ae_spec1.dst_port.push_back(port);
    ae_spec1.terminal = true;
    //ae_spec1.permit_deny = true;
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::PASS;
    ae_spec1.action_l.push_back(action);

    AclEntry *entry1 = new AclEntry();

    PacketHeader *packet1 = new PacketHeader();
    packet1->src_ip = Ip4Address(0x01010102);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    entry1->PopulateAclEntry(ae_spec1);

    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1->PacketMatch(*packet1, &info);
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    //entry1->FindPermitDenyAction(action);
    //EXPECT_TRUE(action);

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    //ASSERT_EQ(1, entry1->Match(packet1));
    //entry1->FindPermitDenyAction(action);
    //EXPECT_TRUE(action);

    packet1->src_ip = Ip4Address(0x01010110);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 11;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010102);
    packet1->protocol = 10;
    //Destination port would be ignored, since protocol is neither TCP or UDP
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010103);
    packet1->protocol = 9;
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010104);
    packet1->protocol = 9;
    packet1->dst_port = 100;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    delete packet1;
    delete entry1;
}



TEST_F(AclEntryTest, BasicDeny) {
    AclEntrySpec ae_spec1;

    ae_spec1.id = 1;
    ae_spec1.BuildAddressInfo("1.1.1.0", 24, &ae_spec1.src_ip_list);
    ae_spec1.src_addr_type = AddressMatch::IP_ADDR;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec port;
    port.min = 10;
    port.max = 100;
    ae_spec1.dst_port.push_back(port);
    ae_spec1.terminal = true;
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::DENY;
    ae_spec1.action_l.push_back(action);
    //ae_spec1.permit_deny = false;

    AclEntry *entry1 = new AclEntry();

    PacketHeader *packet1 = new PacketHeader();
    packet1->src_ip = Ip4Address(0x01010102);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    entry1->PopulateAclEntry(ae_spec1);
    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1->PacketMatch(*packet1, &info);
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::DENY, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));
    //entry1->FindPermitDenyAction(action);
    //EXPECT_FALSE(action);

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::DENY, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));
    //entry1->FindPermitDenyAction(action);
    //EXPECT_FALSE(action);

    packet1->src_ip = Ip4Address(0x01010110);
    packet1->protocol = 10;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::DENY, static_cast<SimpleAction *>(*ial.operator->())->action());
    //EXPECT_TRUE(entry1->Match(packet1));
    //entry1->FindPermitDenyAction(action);
    //EXPECT_FALSE(action);

    packet1->src_ip = Ip4Address(0x01010101);
    packet1->protocol = 11;
    packet1->dst_port = 99;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010102);
    packet1->protocol = 10;
    //Port would be ignored since protocol is not TCP or UDP
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010103);
    packet1->protocol = 9;
    packet1->dst_port = 101;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    packet1->src_ip = Ip4Address(0x01010104);
    packet1->protocol = 9;
    packet1->dst_port = 100;
    al = entry1->PacketMatch(*packet1, &info);
    ial = al.begin();
    EXPECT_EQ(0U, al.size());
    //EXPECT_FALSE(entry1->Match(packet1));

    delete packet1;
    delete entry1;
}

TEST_F(AclEntryTest, BasicIntrospecIpAclEntry) {
    AclEntrySpec ae_spec1;
    ae_spec1.id = 1;
    ae_spec1.BuildAddressInfo("2.2.2.0", 24, &ae_spec1.src_ip_list);
    ae_spec1.src_addr_type = AddressMatch::IP_ADDR;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec port;
    port.min = 10;
    port.max = 100;
    ae_spec1.dst_port.push_back(port);
    ae_spec1.terminal = true;
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::DENY;
    ae_spec1.action_l.push_back(action);

    AclEntry entry1;
    PacketHeader packet1;
    packet1.src_ip = Ip4Address(0x02020202);
    packet1.protocol = 10;
    packet1.dst_port = 99;
    entry1.PopulateAclEntry(ae_spec1);
    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1.PacketMatch(packet1, &info);
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::DENY, static_cast<SimpleAction *>(*ial.operator->())->action());
    AclEntrySandeshData data;
    entry1.SetAclEntrySandeshData(data);
    EXPECT_EQ(data.src_type, "ip");
    EXPECT_EQ(data.dst_port_l.size(), 1);
    EXPECT_EQ(data.dst_port_l.begin()->min, 10);
    EXPECT_EQ(data.dst_port_l.begin()->max, 100);
    EXPECT_EQ(data.proto_l.size(), 1);
    EXPECT_EQ(data.proto_l.begin()->min, 10);
    EXPECT_EQ(data.proto_l.begin()->max, 10);
    EXPECT_EQ(data.src, "2.2.2.0 255.255.255.0");
}

TEST_F(AclEntryTest, BasicIntrospecNetworkAclEntry) {
    AclEntrySpec ae_spec1;
    ae_spec1.id = 1;
    ae_spec1.src_policy_id_str = "network1";
    ae_spec1.src_addr_type = AddressMatch::NETWORK_ID;
    ae_spec1.dst_policy_id_str = "network2";
    ae_spec1.dst_addr_type = AddressMatch::NETWORK_ID;
    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec src_port;
    src_port.min = 100;
    src_port.max = 1000;
    ae_spec1.src_port.push_back(src_port);
    RangeSpec dst_port;
    dst_port.min = 10;
    dst_port.max = 100;
    ae_spec1.dst_port.push_back(dst_port);
    ae_spec1.terminal = true;
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::PASS;
    ae_spec1.action_l.push_back(action);

    AclEntry entry1;
    PacketHeader packet1;
    VnListType src_networks;
    src_networks.insert("network1");
    VnListType dst_networks;
    dst_networks.insert("network2");
    packet1.src_policy_id = &src_networks;
    packet1.dst_policy_id = &dst_networks;
    packet1.protocol = 10;
    packet1.dst_port = 99;
    packet1.src_port  = 101;
    entry1.PopulateAclEntry(ae_spec1);
    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1.PacketMatch(packet1, &info);
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    AclEntrySandeshData data;
    entry1.SetAclEntrySandeshData(data);
    EXPECT_EQ(data.src_type, "network");
    EXPECT_EQ(data.dst_type, "network");
    EXPECT_EQ(data.dst_port_l.size(), 1);
    EXPECT_EQ(data.dst_port_l.begin()->min, 10);
    EXPECT_EQ(data.dst_port_l.begin()->max, 100);
    EXPECT_EQ(data.src_port_l.begin()->min, 100);
    EXPECT_EQ(data.src_port_l.begin()->max, 1000);
    EXPECT_EQ(data.proto_l.size(), 1);
    EXPECT_EQ(data.proto_l.begin()->min, 10);
    EXPECT_EQ(data.proto_l.begin()->max, 10);
    EXPECT_EQ(data.src, "network1");
    EXPECT_EQ(data.dst, "network2");
    EXPECT_EQ(data.ace_id, "1");
}


TEST_F(AclEntryTest, BasicIntrospecSgAclEntry) {
    AclEntrySpec ae_spec1;
    ae_spec1.id = 100;
    ae_spec1.src_sg_id = 4;
    ae_spec1.src_addr_type = AddressMatch::SG;

    ae_spec1.BuildAddressInfo("2.2.2.0", 24, &ae_spec1.dst_ip_list);
    ae_spec1.dst_addr_type = AddressMatch::IP_ADDR;

    RangeSpec protocol;
    protocol.min = 10;
    protocol.max = 10;
    ae_spec1.protocol.push_back(protocol);
    RangeSpec src_port;
    src_port.min = 100;
    src_port.max = 1000;
    ae_spec1.src_port.push_back(src_port);
    RangeSpec dst_port;
    dst_port.min = 10;
    dst_port.max = 100;
    ae_spec1.dst_port.push_back(dst_port);
    ae_spec1.terminal = true;
    ActionSpec action;
    action.ta_type = TrafficAction::SIMPLE_ACTION;
    action.simple_action = TrafficAction::PASS;
    ae_spec1.action_l.push_back(action);

    AclEntry entry1;
    PacketHeader packet1;
    SecurityGroupList src_sg_id_l;
    src_sg_id_l.push_back(4);
    packet1.src_sg_id_l = &src_sg_id_l;
    packet1.dst_ip = Ip4Address(0x02020210);
    packet1.protocol = 10;
    packet1.dst_port = 99;
    packet1.src_port  = 101;
    entry1.PopulateAclEntry(ae_spec1);
    AclEntry::ActionList al;
    FlowPolicyInfo info("");
    al = entry1.PacketMatch(packet1, &info);
    AclEntry::ActionList::iterator ial;
    ial = al.begin();
    EXPECT_EQ(1U, al.size());
    EXPECT_EQ(SimpleAction::PASS, static_cast<SimpleAction *>(*ial.operator->())->action());
    AclEntrySandeshData data;
    entry1.SetAclEntrySandeshData(data);
    EXPECT_EQ(data.src_type, "sg");
    EXPECT_EQ(data.dst_type, "ip");
    EXPECT_EQ(data.dst_port_l.size(), 1);
    EXPECT_EQ(data.dst_port_l.begin()->min, 10);
    EXPECT_EQ(data.dst_port_l.begin()->max, 100);
    EXPECT_EQ(data.src_port_l.begin()->min, 100);
    EXPECT_EQ(data.src_port_l.begin()->max, 1000);
    EXPECT_EQ(data.proto_l.size(), 1);
    EXPECT_EQ(data.proto_l.begin()->min, 10);
    EXPECT_EQ(data.proto_l.begin()->max, 10);
    EXPECT_EQ(data.src, "4");
    EXPECT_EQ(data.dst, "2.2.2.0 255.255.255.0");
    EXPECT_EQ(data.ace_id, "100");
}


} // namespace

int main (int argc, char **argv) {
    LoggingInit();
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
