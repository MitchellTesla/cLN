#ifndef LIGHTNING_LIGHTNINGD_CHANNEL_H
#define LIGHTNING_LIGHTNINGD_CHANNEL_H
#include "config.h"
#include <ccan/list/list.h>
#include <common/channel_id.h>
#include <common/per_peer_state.h>
#include <common/tx_roles.h>
#include <lightningd/channel_state.h>
#include <lightningd/peer_htlcs.h>
#include <wallet/wallet.h>

struct channel_id;
struct uncommitted_channel;
struct wally_psbt;

struct billboard {
	/* Status information to display on listpeers */
	const char *permanent[CHANNEL_STATE_MAX+1];
	const char *transient;
};

struct funding_info {
	struct bitcoin_txid txid;
	u16 outnum;
	u32 feerate;
	struct amount_sat total_funds;

	/* Our original funds, in funding amount */
	struct amount_sat our_funds;
};

struct channel_inflight {
	/* Inside channel->inflights. */
	struct list_node list;

	/* Channel context */
	struct channel *channel;

	/* Funding info */
	const struct funding_info *funding;
	struct wally_psbt *funding_psbt;
	bool remote_tx_sigs;
	bool tx_broadcast;

	/* Commitment tx and sigs */
	struct bitcoin_tx *last_tx;
	struct bitcoin_signature last_sig;

	/* Channel lease infos */
	u32 lease_expiry;
	secp256k1_ecdsa_signature *lease_commit_sig;
	u32 lease_chan_max_msat;
	u16 lease_chan_max_ppt;
};

struct open_attempt {
	/* on uncommitted_channel struct */
	struct channel *channel;
	struct channel_config our_config;
	enum tx_role role;
	bool aborted;

	/* On funding_channel struct */
	struct command *cmd;
	struct amount_sat funding;
	const u8 *our_upfront_shutdown_script;
};

struct channel {
	/* Inside peer->channels. */
	struct list_node list;

	/* Peer context */
	struct peer *peer;

	/* Inflight channel opens */
	struct list_head inflights;

	/* Open attempt */
	struct open_attempt *open_attempt;

	/* Database ID: 0 == not in db yet */
	u64 dbid;

	/* Populated by new_unsaved_channel */
	u64 unsaved_dbid;

	/* Error message (iff in error state) */
	u8 *error;

	/* Their shachain. */
	struct wallet_shachain their_shachain;

 	/* What's happening. */
 	enum channel_state state;

	/* Which side offered channel? */
	enum side opener;

	/* Is there a single subdaemon responsible for us? */
	struct subd *owner;

	/* History */
	struct log *log;
	struct billboard billboard;

	/* Channel flags from opening message. */
	u8 channel_flags;

	/* Our channel config. */
	struct channel_config our_config;

	/* Minimum funding depth (specified by us if they fund). */
	u32 minimum_depth;

	/* Tracking commitment transaction numbers. */
	u64 next_index[NUM_SIDES];
	u64 next_htlc_id;

	/* Funding txid and amounts */
	struct bitcoin_txid funding_txid;
	u16 funding_outnum;
	struct amount_sat funding;

	/* Our original funds, in funding amount */
	struct amount_sat our_funds;

	struct amount_msat push;
	bool remote_funding_locked;
	/* Channel if locked locally. */
	struct short_channel_id *scid;

	struct channel_id cid;

	/* Amount going to us, not counting unfinished HTLCs; if we have one. */
	struct amount_msat our_msat;
	/* Statistics for min and max our_msatoshi. */
	struct amount_msat msat_to_us_min;
	struct amount_msat msat_to_us_max;

	/* Last tx they gave us. */
	struct bitcoin_tx *last_tx;
	enum wallet_tx_type last_tx_type;
	struct bitcoin_signature last_sig;
	const struct bitcoin_signature *last_htlc_sigs;

	/* Keys for channel */
	struct channel_info channel_info;

	/* Fee status */
	const struct fee_states *fee_states;

	/* Our local basepoints */
	struct basepoints local_basepoints;

	/* Our funding tx pubkey. */
	struct pubkey local_funding_pubkey;

	/* scriptpubkey for shutdown, if applicable. */
	const u8 *shutdown_scriptpubkey[NUM_SIDES];
	/* Address for any final outputs */
	u64 final_key_idx;

	/* Amount to give up on each step of the closing fee negotiation. */
	u64 closing_fee_negotiation_step;

	/* Whether closing_fee_negotiation_step is in satoshi or %. */
	u8 closing_fee_negotiation_step_unit;

	/* optional wrong_funding for mutual close */
	const struct bitcoin_outpoint *shutdown_wrong_funding;

	/* Reestablishment stuff: last sent commit and revocation details. */
	bool last_was_revoke;
	struct changed_htlc *last_sent_commit;

	/* Blockheight at creation, scans for funding confirmations
	 * will start here */
	u32 first_blocknum;

	/* Feerate range */
	u32 min_possible_feerate, max_possible_feerate;

	/* Does gossipd need to know if the owner dies? (ie. not onchaind) */
	bool connected;

	/* Do we have an "impossible" future per_commitment_point from
	 * peer via option_data_loss_protect? */
	const struct pubkey *future_per_commitment_point;

	/* Feerate per channel */
	u32 feerate_base, feerate_ppm;

	/* If they used option_upfront_shutdown_script. */
	const u8 *remote_upfront_shutdown_script;

	/* At what commit numbers does `option_static_remotekey` apply? */
	u64 static_remotekey_start[NUM_SIDES];

	/* Was this negotiated with `option_anchor_outputs? */
	bool option_anchor_outputs;

	/* Any commands trying to forget us. */
	struct command **forgets;

	/* Our position in the round-robin list.  */
	u64 rr_number;

	/* the one that initiated a bilateral close, NUM_SIDES if unknown. */
	enum side closer;

	/* Last known state_change cause */
	enum state_change state_change_cause;

	/* Outstanding command for this channel, v2 only */
	struct command *openchannel_signed_cmd;

	/* Block lease expires at, zero is no lease */
	u32 lease_expiry;

	/* Lease commitment, useful someone breaks their promise
	 * wrt channel fees */
	secp256k1_ecdsa_signature *lease_commit_sig;

	/* Lease commited maximum channel fee base msat */
	u32 lease_chan_max_msat;
	/* Lease commited max part per thousandth channel fee (ppm * 1000) */
	u16 lease_chan_max_ppt;
};

/* For v2 opens, a channel that has not yet been committed/saved to disk */
struct channel *new_unsaved_channel(struct peer *peer,
				    u32 feerate_base,
				    u32 feerate_ppm);

struct open_attempt *new_channel_open_attempt(struct channel *channel);

struct channel *new_channel(struct peer *peer, u64 dbid,
			    /* NULL or stolen */
			    struct wallet_shachain *their_shachain STEALS,
			    enum channel_state state,
			    enum side opener,
			    /* NULL or stolen */
			    struct log *log STEALS,
			    const char *transient_billboard TAKES,
			    u8 channel_flags,
			    const struct channel_config *our_config,
			    u32 minimum_depth,
			    u64 next_index_local,
			    u64 next_index_remote,
			    u64 next_htlc_id,
			    const struct bitcoin_txid *funding_txid,
			    u16 funding_outnum,
			    struct amount_sat funding,
			    struct amount_msat push,
			    struct amount_sat our_funds,
			    bool remote_funding_locked,
			    /* NULL or stolen */
			    struct short_channel_id *scid STEALS,
			    struct channel_id *cid,
			    struct amount_msat our_msatoshi,
			    struct amount_msat msatoshi_to_us_min,
			    struct amount_msat msatoshi_to_us_max,
			    struct bitcoin_tx *last_tx STEALS,
			    const struct bitcoin_signature *last_sig,
			    /* NULL or stolen */
			    const struct bitcoin_signature *last_htlc_sigs STEALS,
			    const struct channel_info *channel_info,
			    const struct fee_states *fee_states TAKES,
			    /* NULL or stolen */
			    u8 *remote_shutdown_scriptpubkey STEALS,
			    const u8 *local_shutdown_scriptpubkey,
			    u64 final_key_idx,
			    bool last_was_revoke,
			    /* NULL or stolen */
			    struct changed_htlc *last_sent_commit STEALS,
			    u32 first_blocknum,
			    u32 min_possible_feerate,
			    u32 max_possible_feerate,
			    bool connected,
			    const struct basepoints *local_basepoints,
			    const struct pubkey *local_funding_pubkey,
			    const struct pubkey *future_per_commitment_point,
			    u32 feerate_base,
			    u32 feerate_ppm,
			    /* NULL or stolen */
			    const u8 *remote_upfront_shutdown_script STEALS,
			    u64 local_static_remotekey_start,
			    u64 remote_static_remotekey_start,
			    bool option_anchor_outputs,
			    enum side closer,
			    enum state_change reason,
			    /* NULL or stolen */
			    const struct bitcoin_outpoint *shutdown_wrong_funding STEALS,
			    u32 lease_expiry,
			    secp256k1_ecdsa_signature *lease_commit_sig STEALS,
			    u32 lease_chan_max_msat,
			    u16 lease_chan_max_ppt);

/* new_inflight - Create a new channel_inflight for a channel */
struct channel_inflight *
new_inflight(struct channel *channel,
	     const struct bitcoin_txid funding_txid,
	     u16 funding_outnum,
	     u32 funding_feerate,
	     struct amount_sat funding,
	     struct amount_sat our_funds,
	     struct wally_psbt *funding_psbt STEALS,
	     struct bitcoin_tx *last_tx STEALS,
	     const struct bitcoin_signature last_sig,
	     const u32 lease_expiry,
	     const secp256k1_ecdsa_signature *lease_commit_sig,
	     const u32 lease_chan_max_msat,
	     const u16 lease_chan_max_ppt);

/* Given a txid, find an inflight channel stub. Returns NULL if none found */
struct channel_inflight *channel_inflight_find(struct channel *channel,
					       const struct bitcoin_txid *txid);

/* What's the most recent inflight for this channel? */
struct channel_inflight *
channel_current_inflight(const struct channel *channel);

/* What's the last feerate used for a funding tx on this channel? */
u32 channel_last_funding_feerate(const struct channel *channel);

void delete_channel(struct channel *channel STEALS);

const char *channel_state_name(const struct channel *channel);
const char *channel_state_str(enum channel_state state);

void channel_set_owner(struct channel *channel, struct subd *owner);

/* Channel has failed, but can try again. */
void channel_fail_reconnect(struct channel *channel,
			    const char *fmt, ...) PRINTF_FMT(2,3);
/* Channel has failed, but can try again after a minute. */
void channel_fail_reconnect_later(struct channel *channel,
				  const char *fmt,...) PRINTF_FMT(2,3);

/* Channel has failed, give up on it. */
void channel_fail_permanent(struct channel *channel,
			    enum state_change reason,
			    const char *fmt,
			    ...);
/* Forget the channel. This is only used for the case when we "receive" error
 * during CHANNELD_AWAITING_LOCKIN if we are "fundee". */
void channel_fail_forget(struct channel *channel, const char *fmt, ...);
/* Permanent error, but due to internal problems, not peer. */
void channel_internal_error(struct channel *channel, const char *fmt, ...);

/* Clean up any in-progress commands for a channel */
void channel_cleanup_commands(struct channel *channel, const char *why);

void channel_set_state(struct channel *channel,
		       enum channel_state old_state,
		       enum channel_state state,
		       enum state_change reason,
		       char *why);

const char *channel_change_state_reason_str(enum state_change reason);

/* Find a channel which is not yet saved to disk */
struct channel *peer_unsaved_channel(struct peer *peer);

/* Find a channel which is not onchain, if any */
struct channel *peer_active_channel(struct peer *peer);

/* Find a channel which is in state CHANNELD_NORMAL, if any */
struct channel *peer_normal_channel(struct peer *peer);

/* Get active channel for peer, optionally any uncommitted_channel. */
struct channel *active_channel_by_id(struct lightningd *ld,
				     const struct node_id *id,
				     struct uncommitted_channel **uc);

/* Get unsaved channel for peer */
struct channel *unsaved_channel_by_id(struct lightningd *ld,
				      const struct node_id *id);

struct channel *channel_by_dbid(struct lightningd *ld, const u64 dbid);

struct channel *active_channel_by_scid(struct lightningd *ld,
				       const struct short_channel_id *scid);
struct channel *any_channel_by_scid(struct lightningd *ld,
				    const struct short_channel_id *scid);

/* Get channel by channel_id */
struct channel *channel_by_cid(struct lightningd *ld,
			       const struct channel_id *cid);

void channel_set_last_tx(struct channel *channel,
			 struct bitcoin_tx *tx,
			 const struct bitcoin_signature *sig,
			 enum wallet_tx_type type);

static inline bool channel_can_add_htlc(const struct channel *channel)
{
	return channel->state == CHANNELD_NORMAL;
}

static inline bool channel_fees_can_change(const struct channel *channel)
{
	return channel->state == CHANNELD_NORMAL
		|| channel->state == CHANNELD_SHUTTING_DOWN;
}

static inline bool channel_state_on_chain(enum channel_state state)
{
	return state == ONCHAIN;
}

static inline bool channel_on_chain(const struct channel *channel)
{
	return channel_state_on_chain(channel->state);
}

static inline bool channel_unsaved(const struct channel *channel)
{
	return channel->state == DUALOPEND_OPEN_INIT
		&& channel->dbid == 0;
}

static inline bool channel_active(const struct channel *channel)
{
	return channel->state != FUNDING_SPEND_SEEN
		&& channel->state != CLOSINGD_COMPLETE
		&& !channel_unsaved(channel)
		&& !channel_on_chain(channel);
}

void get_channel_basepoints(struct lightningd *ld,
			    const struct node_id *peer_id,
			    const u64 dbid,
			    struct basepoints *local_basepoints,
			    struct pubkey *local_funding_pubkey);

void channel_set_billboard(struct channel *channel, bool perm,
			   const char *str TAKES);

struct htlc_in *channel_has_htlc_in(struct channel *channel);
struct htlc_out *channel_has_htlc_out(struct channel *channel);

#endif /* LIGHTNING_LIGHTNINGD_CHANNEL_H */
