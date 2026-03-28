/* aria_actor_shim.c — Extended actor patterns: groups, request-reply, broadcast */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <pthread.h>
#include <stdbool.h>

/* Forward declarations from runtime */
extern int64_t aria_shim_actor_spawn(void* behavior_func);
extern int32_t aria_shim_actor_send(int64_t actor_handle, int64_t message);
extern int32_t aria_shim_actor_stop(int64_t actor_handle);
extern int32_t aria_shim_actor_destroy(int64_t actor_handle);
extern int32_t aria_shim_actor_is_alive(int64_t actor_handle);
extern int64_t aria_shim_channel_create(int32_t capacity);
extern int32_t aria_shim_channel_send(int64_t handle, int64_t value);
extern int64_t aria_shim_channel_recv(int64_t handle);
extern int32_t aria_shim_channel_close(int64_t handle);
extern int32_t aria_shim_channel_destroy(int64_t handle);

/* ======================================================================
 * Actor Group — Spawn N actors with the same behavior
 * ====================================================================== */

#define MAX_ACTOR_GROUPS 32
#define MAX_GROUP_SIZE   32

typedef struct {
    int64_t actors[MAX_GROUP_SIZE];
    int32_t count;
} ActorGroup;

static ActorGroup* g_groups[MAX_ACTOR_GROUPS];
static int g_group_count = 0;

/** Create an actor group: N actors with the same behavior. Returns group slot. */
int64_t aria_actpat_group_create(void* behavior_func, int32_t count) {
    if (!behavior_func || count <= 0 || count > MAX_GROUP_SIZE || g_group_count >= MAX_ACTOR_GROUPS)
        return -1;
    ActorGroup* grp = (ActorGroup*)calloc(1, sizeof(ActorGroup));
    if (!grp) return -1;
    grp->count = count;

    for (int32_t i = 0; i < count; i++) {
        grp->actors[i] = aria_shim_actor_spawn(behavior_func);
        if (grp->actors[i] < 0) {
            /* Clean up already spawned */
            for (int32_t j = 0; j < i; j++) {
                aria_shim_actor_destroy(grp->actors[j]);
            }
            free(grp);
            return -1;
        }
    }

    int64_t slot = g_group_count;
    g_groups[slot] = grp;
    g_group_count++;
    return slot;
}

/** Broadcast a message to all actors in the group. Returns 0 on success. */
int32_t aria_actpat_group_broadcast(int64_t slot, int64_t message) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return -1;
    ActorGroup* grp = g_groups[slot];
    int32_t result = 0;
    for (int32_t i = 0; i < grp->count; i++) {
        if (aria_shim_actor_send(grp->actors[i], message) != 0) result = -1;
    }
    return result;
}

/** Send to one actor in the group (round-robin). */
static int32_t g_group_rr[MAX_ACTOR_GROUPS];
int32_t aria_actpat_group_send_rr(int64_t slot, int64_t message) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return -1;
    ActorGroup* grp = g_groups[slot];
    int32_t idx = g_group_rr[slot] % grp->count;
    g_group_rr[slot] = idx + 1;
    return aria_shim_actor_send(grp->actors[idx], message);
}

/** Get the actor handle at index in the group. */
int64_t aria_actpat_group_get_actor(int64_t slot, int32_t index) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return -1;
    ActorGroup* grp = g_groups[slot];
    if (index < 0 || index >= grp->count) return -1;
    return grp->actors[index];
}

/** Get number of actors in group. */
int32_t aria_actpat_group_count(int64_t slot) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return 0;
    return g_groups[slot]->count;
}

/** Count how many actors in the group are still alive. */
int32_t aria_actpat_group_alive_count(int64_t slot) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return 0;
    ActorGroup* grp = g_groups[slot];
    int32_t alive = 0;
    for (int32_t i = 0; i < grp->count; i++) {
        if (aria_shim_actor_is_alive(grp->actors[i]) == 1) alive++;
    }
    return alive;
}

/** Stop all actors in the group. */
int32_t aria_actpat_group_stop_all(int64_t slot) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return -1;
    ActorGroup* grp = g_groups[slot];
    for (int32_t i = 0; i < grp->count; i++) {
        aria_shim_actor_stop(grp->actors[i]);
    }
    return 0;
}

/** Destroy the group and all its actors. */
int32_t aria_actpat_group_destroy(int64_t slot) {
    if (slot < 0 || slot >= MAX_ACTOR_GROUPS || !g_groups[slot]) return -1;
    ActorGroup* grp = g_groups[slot];
    for (int32_t i = 0; i < grp->count; i++) {
        aria_shim_actor_destroy(grp->actors[i]);
    }
    free(grp);
    g_groups[slot] = NULL;
    return 0;
}

/* ======================================================================
 * Request-Reply — Send message to actor and wait for reply on a channel
 * ====================================================================== */

/**
 * Send a request to an actor and wait for a reply.
 * The actor's handler should send a reply to the reply_ch channel.
 * request: the message payload (actor sees this as the message).
 * reply_ch: a channel handle the caller creates, actor sends reply there.
 * timeout_ms: how long to wait for reply (-1 = forever).
 * Returns the reply value.
 *
 * Pattern: caller creates oneshot reply channel, packs (request, reply_ch_handle)
 * as two sequential sends, actor reads both and replies.
 */
int64_t aria_actpat_request_reply(int64_t actor_handle, int64_t request, int64_t reply_ch) {
    /* Send the reply channel handle first, then the request.
     * Actor convention: first recv = reply_ch, second recv = actual message. */
    aria_shim_actor_send(actor_handle, reply_ch);
    aria_shim_actor_send(actor_handle, request);
    /* Wait for reply */
    int64_t reply = aria_shim_channel_recv(reply_ch);
    return reply;
}
