#
# Copyright (c) 2019 Juniper Networks, Inc. All rights reserved.
#

"""Overlay Bgp Feature Implementation."""

from collections import OrderedDict

from abstract_device_api.abstract_device_xsd import (
    Bgp, Feature, Subnet
)

from .db import BgpRouterDM, DataCenterInterconnectDM, \
    GlobalSystemConfigDM, PhysicalRouterDM
from .dm_utils import DMUtils
from .feature_base import FeatureBase


class OverlayBgpFeature(FeatureBase):

    @classmethod
    def feature_name(cls):
        return 'overlay-bgp'
    # end feature_name

    def __init__(self, logger, physical_router, configs):
        """Overlay BGP Feature"""
        super(OverlayBgpFeature, self).__init__(logger, physical_router,
                                                configs)
    # end __init__

    def _is_valid(self, bgp):
        return bgp and bgp.params and bgp.params.get('address')
    # end _is_valid

    def _get_asn(self, bgp):
        return (bgp.params.get('local_autonomous_system') or
                bgp.params.get('autonomous_system'))
    # end _get_asn

    def _add_peer_families(self, config, params):
        if not params.get('family_attributes'):
            return
        family = params['family_attributes'][0].get('address_family')
        if not family:
            return
        if family in ['e-vpn', 'e_vpn']:
            family = 'evpn'
        config.add_families(family)
    # end _add_peer_families

    def _add_families(self, config, params):
        if not params.get('address_families'):
            return
        families = params['address_families'].get('family', [])
        if not families:
            return
        for family in families:
            if family in ['e-vpn', 'e_vpn']:
                family = 'evpn'
            config.add_families(family)
    # end _add_families

    def _add_auth_config(self, config, params):
        if not params.get('auth_data'):
            return
        keys = params['auth_data'].get('key_items', [])
        if len(keys) > 0:
            config.set_authentication_key(keys[0].get('key'))
    # end _add_auth_config

    def _add_hold_time_config(self, config, params):
        if params.get('hold_time') is None:
            return
        config.set_hold_time(params.get('hold_time'))
    # end _add_hold_time_config

    def _get_config(self, bgp, external=False,
                    is_RR=False, name=None):
        config = Bgp()

        cluster_id = bgp.params.get('cluster_id')
        if cluster_id and not is_RR:
            config.set_cluster_id(cluster_id)
        if name is None:
            config.set_name(DMUtils.make_bgp_group_name(
                self._get_asn(bgp), external, is_RR))
        else:
            config.set_name(name)
        config.set_type('external' if external else 'internal')

        config.set_ip_address(bgp.params['address'])
        config.set_autonomous_system(self._get_asn(bgp))

        self._add_families(config, bgp.params)
        self._add_auth_config(config, bgp.params)
        self._add_hold_time_config(config, bgp.params)

        return config
    # end _get_config

    def _update_config_from_session(self, config, attr):
        sessions = attr.get('session')
        if not sessions:
            return
        # for now assume only one session
        session_attrs = sessions[0].get('attributes', [])
        for session_attr in session_attrs:
            # For now, only consider the attribute if bgp-router is
            # not specified
            if not session_attr.get('bgp_router'):
                continue

            self._add_peer_families(config, session_attr)
            self._add_auth_config(config, session_attr)
            break
    # end _update_config_from_session

    def _add_peers(self, config, bgp, peers):
        for peer, attr in list(peers.items()):
            peer_config = Bgp()
            peer_config.set_ip_address(peer.params['address'])
            peer_config.set_autonomous_system(self._get_asn(peer))
            self._update_config_from_session(peer_config, attr)

            # This peer_config comment will be used to ignore
            # enabling bfd towards the control node
            if peer.params.get('router_type') == 'control-node':
                peer_config.set_comment("Control Node")

            config.add_peers(peer_config)
    # end _add_peers

    def _build_dci_bgp_groups(self, local_asn):
        dci_bgp_groups = {}
        neigh_bgpr_dict, dci_dict = \
            DataCenterInterconnectDM.get_dci_peers(self._physical_router)

        # create dci bgp group having common peer bgp routers
        for dci_peer_uuid, dict_values in neigh_bgpr_dict.items():
            peer = BgpRouterDM.get(dci_peer_uuid)
            if not self._is_valid(peer):
                continue
            postfix = ''
            if local_asn != self._get_asn(peer):
                postfix = '-e'
            else:
                postfix = '-i'
            dci_name_list = list(dict_values['dci_names'])
            # using peer_fabric name as a bgp group.
            bgp_gname = DMUtils.contrail_prefix(dict_values['fabric']) + \
                postfix
            if bgp_gname not in dci_bgp_groups:
                dci_bgp_groups[bgp_gname] = {
                    'peer_bgp_routers': OrderedDict(),
                    'import_policy': set(),
                    'policies': set()
                }

            for dci_name in dci_name_list:
                if dci_name not in dci_dict:
                    continue
                dci_bgp_groups[bgp_gname]['peer_bgp_routers'][peer] = {}
                # make sure no duplicate policies added per bgp group
                if 'policies' in dci_dict[dci_name]:
                    for i_policy in dci_dict[dci_name]['policies'] or []:
                        if i_policy.get_name() not in \
                                dci_bgp_groups[bgp_gname]['import_policy']:
                            dci_bgp_groups[bgp_gname]['policies'].add(
                                i_policy)
                if 'import_policy' in dci_dict[dci_name]:
                    dci_bgp_groups[bgp_gname]['import_policy'].update(
                        dci_dict[dci_name]['import_policy'])

        return dci_bgp_groups, neigh_bgpr_dict
    # end _build_dci_bgp_groups

    def _build_bgp_config(self, feature_config):
        bgp_router = BgpRouterDM.get(self._physical_router.bgp_router)
        if not self._is_valid(bgp_router):
            return

        ibgp_peers = OrderedDict()
        ebgp_peers = OrderedDict()
        rr_peers = OrderedDict()
        local_asn = self._get_asn(bgp_router)

        dci_bgp_groups, neigh_bgpr_dict = self._build_dci_bgp_groups(
            local_asn)
        bgp_peers_dict = bgp_router.bgp_routers

        for peer_uuid, attr in list(bgp_peers_dict.items()):
            if peer_uuid in neigh_bgpr_dict:
                continue
            peer = BgpRouterDM.get(peer_uuid)
            if not self._is_valid(peer):
                continue
            peer_pr_uuid = peer.physical_router
            peer_pr = PhysicalRouterDM.get(peer_pr_uuid)
            peer_asn = self._get_asn(peer)

            if local_asn != peer_asn:
                ebgp_peers[peer] = attr
            elif peer_pr and "Route-Reflector" in \
                    peer_pr.routing_bridging_roles \
                    and "Route-Reflector" in \
                    self._physical_router.routing_bridging_roles:
                rr_peers[peer] = attr
            # This is needed as RB role "lean" does not have overlay bgp
            # config so if peer is lean only donot create iBGP session
            elif peer_pr and len(peer_pr.routing_bridging_roles) == 1 \
                    and peer_pr.routing_bridging_roles[-1] == "lean":
                continue
            else:
                ibgp_peers[peer] = attr

        # Add ibgp group, even when no peers
        ibgp = self._get_config(bgp_router)
        self._add_peers(ibgp, bgp_router, ibgp_peers)
        feature_config.add_bgp(ibgp)

        if ebgp_peers:
            ebgp = self._get_config(bgp_router, external=True)
            self._add_peers(ebgp, bgp_router, ebgp_peers)
            feature_config.add_bgp(ebgp)

        if rr_peers:
            rr = self._get_config(bgp_router, is_RR=True)
            self._add_peers(rr, bgp_router, rr_peers)
            feature_config.add_bgp(rr)

        for dci_bgp_name, attr in list(dci_bgp_groups.items()):
            dci_bgp = self._get_config(bgp_router,
                                       external=dci_bgp_name.endswith('-e'),
                                       is_RR=dci_bgp_name.endswith('-rr'),
                                       name=dci_bgp_name)
            self._add_peers(dci_bgp, bgp_router, attr['peer_bgp_routers'])
            if len(attr['import_policy']) > 0:
                dci_bgp.set_import_policy(list(attr['import_policy']))
            if len(attr['policies']) > 0:
                dci_bgp.set_policies(list(attr['policies']))
            feature_config.add_bgp(dci_bgp)

    # end _build_bgp_config

    def _add_dynamic_tunnels(self, feature_config, tunnel_source_ip,
                             ip_fabric_subnets):
        feature_config.set_tunnel_ip(tunnel_source_ip)
        if not ip_fabric_subnets:
            return
        for subnet in ip_fabric_subnets.get('subnet', []):
            dest_net = Subnet(prefix=subnet['ip_prefix'],
                              prefix_len=subnet['ip_prefix_len'])
            feature_config.add_tunnel_destination_networks(dest_net)
    # end _add_dynamic_tunnels

    def _build_dynamic_tunnels_config(self, feature_config):
        bgp_router = BgpRouterDM.get(self._physical_router.bgp_router)
        tunnel_ip = self._physical_router.dataplane_ip or \
            (bgp_router.params.get('address')
                if self._is_valid(bgp_router) else None)
        if tunnel_ip and self._physical_router.is_valid_ip(tunnel_ip):
            self._add_dynamic_tunnels(
                feature_config, tunnel_ip,
                GlobalSystemConfigDM.ip_fabric_subnets)
    # end _build_dynamic_tunnels_config

    def feature_config(self, **kwargs):
        feature_config = Feature(name=self.feature_name())
        self._build_bgp_config(feature_config)
        self._build_dynamic_tunnels_config(feature_config)
        return feature_config
    # end push_conf

# end OverlayBgpFeature
