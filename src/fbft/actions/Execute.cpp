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

Execute::Execute(Blockchain& blockchain, RoastWallet& wallet, PlTerm Replica_id, PlTerm Req_digest, PlTerm V,
                 PlTerm N)
    : Action(Replica_id), m_blockchain(blockchain), m_wallet(wallet) {
  m_view = (long)V;
  m_seq_number = (long)N;

  // Retrieve payload from the msg store
  std::string req_digest = (char*)Req_digest;
  Request r = Request::FindByDigest(m_replica_id, req_digest);

  m_request = r;
}

std::vector<std::unique_ptr<actions::Execute>>
Execute::BuildActives(const itcoin::FbftConfig& config, Blockchain& blockchain, RoastWallet& wallet) {
  std::vector<unique_ptr<actions::Execute>> results{};
  PlTerm Req_digest, V, N, Replica_id{(long)config.id()};
  PlQuery query("pre_EXECUTE", PlTermv(Req_digest, V, N, Replica_id));
  while (query.next_solution()) {
    actions::Execute action{blockchain, wallet, Replica_id, Req_digest, V, N};
    std::unique_ptr<actions::Execute> p_action = std::make_unique<actions::Execute>(action);
    results.emplace_back(move(p_action));
  }
  return results;
}

int Execute::effect() const {
  std::vector<uint32_t> signers;
  std::vector<string> signatures;

  // ROAST-only
  string pre_signature;

  // We need to find out the block to signatures
  PlTerm Replica_id{(long)m_replica_id}, Session_id, Signers, Pre_signature, Signature_shares;
  PlQuery query{"roast_final_signature_session",
                PlTermv(Replica_id, Session_id, Signers, Pre_signature, Signature_shares)};
  while (query.next_solution()) {
    PlTail SignersTail{Signers};
    PlTerm SignerElem;
    while (SignersTail.next(SignerElem)) {
      signers.emplace_back((long)SignerElem);
    }
    PlTail SigShareTail{Signature_shares};
    PlTerm SigShareElem;
    while (SigShareTail.next(SigShareElem)) {
      string sig_share = (const char*)SigShareElem;
      signatures.emplace_back(sig_share);
    }
    pre_signature = (const char*)Pre_signature;
  }

  BOOST_LOG_TRIVIAL(trace) << str(boost::format("%1% effect(), Aggregating the following signatures:") %
                                  this->identify());
  for (int i = 0; i < signers.size(); i++) // access by reference to avoid copying
  {
    BOOST_LOG_TRIVIAL(trace) << str(boost::format("%1% effect(), signature from R%2% = %3%") %
                                    this->identify() % signers.at(i) % signatures.at(i));
  }

  // Retrieve thre proposed block
  PrePrepare ppp_msg = PrePrepare::FindByV_N_Req(m_replica_id, m_view, m_seq_number, m_request.digest());
  CBlock proposed_block = ppp_msg.proposed_block();
  CBlock final_block;

  // Cast the wallet to a FROST wallet, otherwise ROAST cannot work
  RoastWallet& wallet = dynamic_cast<wallet::RoastWallet&>(m_wallet);
  final_block = wallet.FinalizeBlock(proposed_block, pre_signature, signatures);

  if (final_block.GetHash() != proposed_block.GetHash()) {
    BOOST_LOG_TRIVIAL(error) << "The executed block has mismatching hash, and will be ignored.";
    return 0;
  }

  BOOST_LOG_TRIVIAL(trace) << str(boost::format("%1% effect(), Proposed/Final block hash = %2%>") %
                                  this->identify() % final_block.GetHash().GetHex());

  BOOST_LOG_TRIVIAL(trace) << str(boost::format("%1% effect(), Proposed block = %2%>") % this->identify() %
                                  HexSerializableCBlock(proposed_block).GetHex());

  BOOST_LOG_TRIVIAL(trace) << str(boost::format("%1% effect(), Final block = %2%>") % this->identify() %
                                  HexSerializableCBlock(final_block).GetHex());

  m_blockchain.SubmitBlock(m_seq_number, final_block);

  PlTermv args(PlString((const char*)m_request.digest().c_str()), PlTerm((long)m_seq_number),
               PlString((const char*)proposed_block.GetHash().GetHex().c_str()), PlTerm((long)m_replica_id));
  return PlCall("effect_EXECUTE", args);
}

std::string Execute::identify() const {
  return str(boost::format("<%1%, Request=%2%, V=%3%, N=%4%, R=%5%>") % name() % m_request.digest() % m_view %
             m_seq_number % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
