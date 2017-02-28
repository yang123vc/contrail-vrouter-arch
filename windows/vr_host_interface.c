#include "precomp.h"

#include "vr_interface.h"
#include "vr_packet.h"
#include "vr_windows.h"

extern PSX_SWITCH_OBJECT SxSwitchObject;
static PNDIS_RW_LOCK_EX win_if_mutex;
static LOCK_STATE_EX win_if_mutex_state;

static void
win_if_lock(void)
{
    NdisAcquireRWLockWrite(win_if_mutex, &win_if_mutex_state, 0);
}

static void
win_if_unlock(void)
{
    NdisReleaseRWLock(win_if_mutex, &win_if_mutex_state);
}

static int
win_if_add(struct vr_interface* vif)
{
    if (vif->vif_type == VIF_TYPE_STATS)
        return 0;

    if (vif->vif_name[0] == '\0')
        return -ENODEV;

    // Unlike FreeBSD/Linux, we don't have to register handlers here

    return 0;
}

static int
win_if_add_tap(struct vr_interface* vif)
{
    UNREFERENCED_PARAMETER(vif);
    // NOOP - no bridges on Windows
    return 0;
}

static int
win_if_del(struct vr_interface *vif)
{
    UNREFERENCED_PARAMETER(vif);

    /* TODO: Implement (JW-139) */
    DbgPrint("%s(): dummy implementation called\n", __func__);

    return 0;
}

static int
win_if_del_tap(struct vr_interface *vif)
{
    UNREFERENCED_PARAMETER(vif);

    /* TODO: Implement (JW-139) */
    DbgPrint("%s(): dummy implementation called\n", __func__);

    return 0;
}

static int
win_if_tx(struct vr_interface *vif, struct vr_packet* pkt)
{
    if (vif == NULL)
        return 0; // Sent into /dev/null

    PNET_BUFFER_LIST nbl = pkt->vp_net_buffer_list;

    /*PNDIS_SWITCH_FORWARDING_DESTINATION_ARRAY dests = NULL;
    SxSwitchObject->NdisSwitchHandlers.GetNetBufferListDestinations(SxSwitchObject->NdisSwitchContext, nbl, &dests);

    if (!dests)
        return -EFAULT;

    // If there are any destinations already, disable them.
    if (dests->NumDestinations != 0)
    {
        PNDIS_SWITCH_PORT_DESTINATION dest = dests->FirstElement;
        for (int i = 0; i < dests->NumElements; i++)
        {
            dest->IsExcluded = true;
            dest = dest + 1;
        }
    }

    if (dests->NumElements - dests->NumDestinations == 0) // If there is no space for more destinations, we have to grow the buffer;
        SxSwitchObject->NdisSwitchHandlers.GrowNetBufferListDestinations(
            SxSwitchObject->NdisSwitchContext, nbl, 1, &dests);*/

    PNDIS_SWITCH_PORT_DESTINATION dest = ExAllocatePoolWithTag(NonPagedPool, sizeof(NDIS_SWITCH_PORT_DESTINATION), SxExtAllocationTag); // = NDIS_SWITCH_PORT_DESTINATION_AT_ARRAY_INDEX(dests, dests->NumDestinations);
    dest->IsExcluded = 0;
    dest->NicIndex = vif->vif_nic;
    dest->PortId = vif->vif_port;
    dest->PreservePriority = true;
    dest->PreserveVLAN = true;

    SxSwitchObject->NdisSwitchHandlers.AddNetBufferListDestination(SxSwitchObject->NdisFilterHandle, nbl, dest);

    int injected = (nbl->SourceHandle == SxSwitchObject->NdisFilterHandle) ? 1 : 0;

    // We are sure there is only 1 packet so all packets have a single source. Same is true for destinations.
    SxLibSendNetBufferListsIngress(SxSwitchObject, nbl, NDIS_SEND_FLAGS_SWITCH_SINGLE_SOURCE | NDIS_SEND_FLAGS_SWITCH_DESTINATION_GROUP, injected);

    return 0;
}

static int
win_if_rx(struct vr_interface *vif, struct vr_packet* pkt)
{
    UNREFERENCED_PARAMETER(vif);
    UNREFERENCED_PARAMETER(pkt);

    /* NOOP ATM. Only used in vhosts, which are not supported */

    return 0;
}

static int
win_if_get_settings(struct vr_interface *vif, struct vr_interface_settings *settings)
{
    UNREFERENCED_PARAMETER(vif);
    UNREFERENCED_PARAMETER(settings);

    /* TODO: Implement */
    DbgPrint("%s(): dummy implementation called\n", __func__);

    return -EINVAL;
}

static unsigned int
win_if_get_mtu(struct vr_interface *vif)
{
    UNREFERENCED_PARAMETER(vif);

    /* TODO: Implement */
    DbgPrint("%s(): dummy implementation called\n", __func__);

    return vif->vif_mtu;
}

static unsigned short
win_if_get_encap(struct vr_interface *vif)
{
    UNREFERENCED_PARAMETER(vif);

    /* TODO: Implement */
    DbgPrint("%s(): dummy implementation called\n", __func__);

    return VIF_ENCAP_TYPE_ETHER;
}

static struct vr_host_interface_ops win_host_interface_ops = {
    .hif_lock           = win_if_lock,
    .hif_unlock         = win_if_unlock,
    .hif_add            = win_if_add,
    .hif_del            = win_if_del,
    .hif_add_tap        = win_if_add_tap,
    .hif_del_tap        = win_if_del_tap,
    .hif_tx             = win_if_tx,
    .hif_rx             = win_if_rx,
    .hif_get_settings   = win_if_get_settings,
    .hif_get_mtu        = win_if_get_mtu,
    .hif_get_encap      = win_if_get_encap,
    .hif_stats_update   = NULL,
};

void
vr_host_vif_init(struct vrouter *router)
{
    UNREFERENCED_PARAMETER(router);
}

void
vr_host_interface_exit(void)
{
    NdisFreeRWLock(win_if_mutex);
}

struct vr_host_interface_ops *
vr_host_interface_init(void)
{
    win_if_mutex = NdisAllocateRWLock(SxSwitchObject->NdisFilterHandle);
    if (!win_if_mutex) {
        DbgPrint("%s(): RWLock could not be allocated\n", __func__);
        return NULL;
    }

    return &win_host_interface_ops;
}
