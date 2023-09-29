// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#include "actions.h"

#include <iterator>

#include <SWI-cpp.h>
#include <boost/format.hpp>
#include <boost/log/trivial.hpp>

#include "../../wallet/wallet.h"

using namespace std;
using namespace itcoin::fbft::messages;
using namespace itcoin::wallet;

namespace itcoin {
namespace fbft {
namespace actions {

RoastReceivePreSignature::RoastReceivePreSignature(wallet::RoastWallet& wallet, uint32_t replica_id,
                                                   messages::RoastPreSignature msg)
    : Action(replica_id), m_wallet(wallet), m_msg(msg){

                                            };

RoastReceivePreSignature::~RoastReceivePreSignature(){

};

int RoastReceivePreSignature::effect() const {
  CBlock block_to_sign;
  PlTerm Replica_id{(long)m_replica_id}, V, N, Req_digest;
  int result = PlCall("roast_active", PlTermv(Replica_id, V, N, Req_digest));
  if (result) {
    uint32_t v = (long)V;
    uint32_t n = (long)N;
    string req_digest = (const char*)Req_digest;
    PrePrepare ppp_msg = PrePrepare::FindByV_N_Req(m_replica_id, v, n, req_digest);
    block_to_sign = ppp_msg.proposed_block();
  } else {
    string error_msg =
        str(boost::format("R%1% received PRE_SIGNATURE but ROAST is not active, it will be ignored.") %
            m_replica_id);
    BOOST_LOG_TRIVIAL(error) << error_msg;
    return 0;
  }

  bool replica_id_found = false;
  for (uint32_t signer_id : m_msg.signers()) {
    if (signer_id == m_replica_id) {
      replica_id_found = true;
      break;
    }
  }

  if (replica_id_found) {
    string signature_share =
        m_wallet.GetSignatureShare(m_msg.signers(), m_msg.pre_signature(), block_to_sign);
    string next_pre_sig_share = m_wallet.GetPreSignatureShare();

    PlTermv args(PlTerm((long)m_replica_id), m_msg.signers_as_plterm(),
                 PlString((const char*)m_msg.pre_signature().c_str()),
                 PlString((const char*)signature_share.c_str()),
                 PlString((const char*)next_pre_sig_share.c_str()));
    return PlCall("effect_RECEIVE_PRE_SIGNATURE", args);
  } else {
    BOOST_LOG_TRIVIAL(info) << str(
        boost::format(
            "R%1% received PRE_SIGNATURE but it is not part of the selected signers, it will be ignored.") %
        m_replica_id);
    return 0;
  }
}

std::string RoastReceivePreSignature::identify() const {
  return str(boost::format("<%1%, msg=%2%, R=%3%>") % name() % m_msg.identify() % m_replica_id);
}

} // namespace actions
} // namespace fbft
} // namespace itcoin
