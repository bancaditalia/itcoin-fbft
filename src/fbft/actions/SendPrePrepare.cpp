// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <SWI-cpp.h>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../../blockchain/blockchain.h"
#include "../../wallet/wallet.h"
#include "config/FbftConfig.h"

using namespace std;
using namespace itcoin::blockchain;
using namespace itcoin::wallet;
using namespace itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

SendPrePrepare::SendPrePrepare(Blockchain& blockchain, PlTerm Replica_id, PlTerm Req_digest, PlTerm V,
                               PlTerm N)
    : Action(Replica_id), m_blockchain(blockchain) {
  m_view = (long)V;
  m_seq_number = (long)N;

  // Retrieve request
  std::string req_digest = (char*)Req_digest;
  m_request = messages::Request::FindByDigest(m_replica_id, req_digest);
}

std::vector<std::unique_ptr<actions::SendPrePrepare>>
SendPrePrepare::BuildActives(const itcoin::FbftConfig& config, Blockchain& blockchain, RoastWallet& wallet) {
  std::vector<unique_ptr<actions::SendPrePrepare>> results{};
  PlTerm Req_digest, V, N, Replica_id{(long)config.id()};
  PlQuery query("pre_SEND_PRE_PREPARE", PlTermv(Req_digest, V, N, Replica_id));
  while (query.next_solution()) {
    std::unique_ptr<actions::SendPrePrepare> action =
        std::make_unique<actions::SendPrePrepare>(blockchain, Replica_id, Req_digest, V, N);
    results.emplace_back(std::move(action));
  }
  return results;
}

int SendPrePrepare::effect() const {
  // We create the block to propose
  CBlock proposed_block;
  try {
    proposed_block = m_blockchain.GenerateBlock(m_request.timestamp());
  } catch (const std::runtime_error& e) {
    BOOST_LOG_TRIVIAL(error) << str(
        boost::format("%1% effect(), GenerateBlock raised runtime exception = %2%") % this->identify() %
        e.what());
    return 0;
  }

  const auto block_ser = HexSerializableCBlock(proposed_block);
  const uint32_t block_size_bytes = block_ser.GetHex().length() / 2;
  const std::string block_hash = proposed_block.GetBlockHeader().GetHash().ToString();

  BOOST_LOG_TRIVIAL(debug) << str(boost::format("%1% effect(), Proposed block size: %2% bytes, hash: %3%") %
                                  this->identify() % block_size_bytes % block_hash);

  // Now the PrePrepare is ready to be sent, we can save it into the message store
  messages::PrePrepare msg(m_replica_id, m_view, m_seq_number, m_request.digest().c_str(), proposed_block);

  PlTermv args(PlString((const char*)m_request.digest().c_str()),
               PlString((const char*)msg.proposed_block_hex().c_str()), PlTerm((long)m_view),
               PlTerm((long)m_seq_number), PlTerm((long)m_replica_id));
  return PlCall("effect_SEND_PRE_PREPARE", args);
}

std::string SendPrePrepare::identify() const {
  return str(boost::format("<%1%, Request=%2%, V=%3%, N=%4%, R=%5%>") % name() % m_request.digest() % m_view %
             m_seq_number % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
