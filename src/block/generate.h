#ifndef ITCOIN_BLOCK_GENERATE_H
#define ITCOIN_BLOCK_GENERATE_H

#include <primitives/block.h>

namespace itcoin { namespace transport {
  class BtcClient;
}} // namespace itcoin::transport

namespace itcoin { namespace block {

const std::vector<unsigned char> SIGNET_HEADER_VEC = std::vector<unsigned char>{0xec, 0xc7, 0xda, 0xa2};

/**
 * This function generates a itcoin-flavoured signet block.
 *
 * The coinbase transaction will be sent to rewardAddress. The content of the specific
 * block (for example, the signet challenge) depends on the configuration of the
 * itcoin node pointed to by bitcoindClient
 *
 * Sub-passes:
 * - block template generation
 * - building of the coinbase transaction
 * - block creation
 * - appending of the witness commitment to the scriptPubKey
 * - appending of the SIGNET_HEADER to the scriptPubKey
 * - mining of the block
 *
 * TODO: link to somewhere in the documentation where we explain that in itcoin
 *       the Merkle hash does not include the signatures).
 *
 * @param bitcoindClient the JSON-RPC Bitcoin client wrapper
 * @param address the reward address for the coinbase transaction
 * @return the generated block
 */
CBlock generateBlock(transport::BtcClient& bitcoindClient, const std::string& address, uint32_t block_timestamp);

/**
 * Get the scriptPubKey of a Bitcoin address.
 *
 * @param bitcoindClient the JSON-RPC Bitcoin client wrapper
 * @param address the address whose scriptPubKey we ask for
 * @return the scriptPubKey, as a CScript object
 */
CScript getScriptPubKey(transport::BtcClient& bitcoindClient, const std::string& address);

/**
 * Get the Witness script.
 *
 * @param witnessRoot the witness of the Merkle root
 * @param witnessNonce the witness nonce
 * @return the combined witness script
 */
CScript GetWitnessScript(uint256 witnessRoot, uint256 witnessNonce);

}} // namespace itcoin::block

#endif // ITCOIN_BLOCK_GENERATE_H
