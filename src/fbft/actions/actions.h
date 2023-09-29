// Copyright (c) 2023 Bank of Italy
// Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

#ifndef ITCOIN_FBFT_ACTIONS_ACTIONS_H
#define ITCOIN_FBFT_ACTIONS_ACTIONS_H

#include <SWI-cpp.h>

#include "../messages/messages.h"

namespace itcoin {
enum SIGNATURE_ALGO_TYPE : unsigned int;
class FbftConfig;
} // namespace itcoin

namespace itcoin {
namespace blockchain {
class Blockchain;
}
} // namespace itcoin

namespace itcoin {
namespace wallet {
class RoastWallet;
}
} // namespace itcoin

namespace blockchain = itcoin::blockchain;
namespace wallet = itcoin::wallet;
namespace messages = itcoin::fbft::messages;

namespace itcoin {
namespace fbft {
namespace actions {

enum class ACTION_TYPE {
  INVALID = 0,
  EXECUTE = 1,
  PROCESS_NEW_VIEW = 2,
  RECEIVE_BLOCK = 3,
  RECEIVE_COMMIT = 4,
  RECEIVE_NEW_VIEW = 5,
  RECEIVE_PREPARE = 6,
  RECEIVE_PRE_PREPARE = 7,
  RECEIVE_REQUEST = 8,
  RECEIVE_VIEW_CHANGE = 9,
  RECOVER_VIEW = 10,
  SEND_COMMIT = 11,
  SEND_NEW_VIEW = 12,
  SEND_PREPARE = 13,
  SEND_PRE_PREPARE = 14,
  SEND_VIEW_CHANGE = 15,
  ROAST_INIT = 16,
  ROAST_RECEIVE_PRE_SIGNATURE = 17,
  ROAST_RECEIVE_SIGNATURE_SHARE = 18,
};

const std::string ACTION_TYPE_TYPE_AS_STRING[] = {
    "INVALID",
    "EXECUTE",
    "PROCESS_NEW_VIEW",
    "RECEIVE_BLOCK",
    "RECEIVE_COMMIT",
    "RECEIVE_NEW_VIEW",
    "RECEIVE_PREPARE",
    "RECEIVE_PRE_PREPARE",
    "RECEIVE_REQUEST",
    "RECEIVE_VIEW_CHANGE",
    "RECOVER_VIEW",
    "SEND_COMMIT",
    "SEND_NEW_VIEW",
    "SEND_PREPARE",
    "SEND_PRE_PREPARE",
    "SEND_VIEW_CHANGE",
    "ROAST_INIT",
    "ROAST_RECEIVE_PRE_SIGNATURE",
    "ROAST_RECEIVE_SIGNATURE_SHARE",
};

class Action {
public:
  Action(uint32_t replica_id);
  Action(PlTerm Replica_id);
  virtual ~Action(){};

  // Getters
  virtual std::string identify() const = 0;
  virtual std::optional<std::reference_wrapper<const messages::Message>> message() const;
  std::string name() const;
  virtual ACTION_TYPE type() const = 0;

  // Operations
  virtual int effect() const = 0;

  // Operators
  friend std::ostream& operator<<(std::ostream& Str, const Action& action);

protected:
  uint32_t m_replica_id;
};

class Execute : public Action {
public:
  Execute(blockchain::Blockchain& blockchain, wallet::RoastWallet& wallet, PlTerm Replica_id,
          PlTerm Req_digest, PlTerm V, PlTerm N);
  ~Execute(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::EXECUTE; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::Execute>> BuildActives(const itcoin::FbftConfig& config,
                                                                     blockchain::Blockchain& blockchain,
                                                                     wallet::RoastWallet& wallet);

private:
  blockchain::Blockchain& m_blockchain;
  wallet::RoastWallet& m_wallet;

  messages::Request m_request;
  uint32_t m_view;
  uint32_t m_seq_number;
};

class ProcessNewView : public Action {
public:
  ProcessNewView(PlTerm Replica_id, PlTerm Hi, PlTerm Nu, PlTerm Chi);
  ~ProcessNewView(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::PROCESS_NEW_VIEW; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::ProcessNewView>>
  BuildActives(const itcoin::FbftConfig& config, blockchain::Blockchain& blockchain,
               wallet::RoastWallet& wallet);

private:
  uint32_t m_hi;
  messages::new_view_nu_t m_nu;
  messages::new_view_chi_t m_chi;
};

class ReceiveBlock : public Action {
public:
  ReceiveBlock(uint32_t replica_id, messages::Block msg) : Action(replica_id), m_msg(msg){};
  ~ReceiveBlock(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_BLOCK; }

  int effect() const;

private:
  messages::Block m_msg;
};

class ReceiveCommit : public Action {
public:
  ReceiveCommit(uint32_t replica_id, messages::Commit msg) : Action(replica_id), m_msg(msg){};
  ~ReceiveCommit(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_COMMIT; }

  int effect() const;

private:
  messages::Commit m_msg;
};

class ReceiveNewView : public Action {
public:
  ReceiveNewView(wallet::RoastWallet& wallet, uint32_t replica_id, messages::NewView msg);
  ~ReceiveNewView(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_NEW_VIEW; }

  int effect() const;

private:
  wallet::RoastWallet& m_wallet;

  messages::NewView m_msg;
};

class ReceivePrepare : public Action {
public:
  ReceivePrepare(uint32_t replica_id, messages::Prepare msg) : Action(replica_id), m_msg(msg){};
  ;
  ~ReceivePrepare(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_PREPARE; }

  int effect() const;

private:
  messages::Prepare m_msg;
};

class ReceivePrePrepare : public Action {
public:
  ReceivePrePrepare(uint32_t replica_id, blockchain::Blockchain& blockchain, double current_time,
                    double pre_prepare_time_tolerance_delta, messages::PrePrepare msg);
  ~ReceivePrePrepare(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_PRE_PREPARE; }

  int effect() const;

private:
  blockchain::Blockchain& m_blockchain;
  double m_current_time;
  double m_pre_prepare_time_tolerance_delta;
  messages::PrePrepare m_msg;
};

class ReceiveRequest : public Action {
public:
  ReceiveRequest(uint32_t replica_id, messages::Request request) : Action(replica_id), m_msg(request){};
  ~ReceiveRequest(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_REQUEST; }

  int effect() const;

private:
  messages::Request m_msg;
};

class ReceiveViewChange : public Action {
public:
  ReceiveViewChange(uint32_t replica_id, messages::ViewChange msg) : Action(replica_id), m_msg(msg){};
  ~ReceiveViewChange(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::RECEIVE_VIEW_CHANGE; }

  int effect() const;

private:
  messages::ViewChange m_msg;
};

class RecoverView : public Action {
public:
  RecoverView(uint32_t replica_id, uint32_t view);
  ~RecoverView();

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::RECOVER_VIEW; }
  int effect() const;

  static std::vector<std::unique_ptr<actions::RecoverView>> BuildActives(const itcoin::FbftConfig& config,
                                                                         blockchain::Blockchain& blockchain,
                                                                         wallet::RoastWallet& wallet);

private:
  uint32_t m_view;
};

class SendCommit : public Action {
public:
  SendCommit(wallet::RoastWallet& wallet, PlTerm Replica_id, PlTerm Req_digest, PlTerm V, PlTerm N);
  ~SendCommit(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::SEND_COMMIT; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::SendCommit>> BuildActives(const itcoin::FbftConfig& config,
                                                                        blockchain::Blockchain& blockchain,
                                                                        wallet::RoastWallet& wallet);

private:
  wallet::RoastWallet& m_wallet;

  messages::Request m_request;
  uint32_t m_view;
  uint32_t m_seq_number;
};

class SendNewView : public Action {
public:
  SendNewView(uint32_t replica_id, messages::new_view_nu_t nu, messages::new_view_chi_t chi)
      : Action(replica_id), m_nu(nu), m_chi(chi){};
  ~SendNewView(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::SEND_NEW_VIEW; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::SendNewView>> BuildActives(const itcoin::FbftConfig& config,
                                                                         blockchain::Blockchain& blockchain,
                                                                         wallet::RoastWallet& wallet);

private:
  messages::new_view_nu_t m_nu;
  messages::new_view_chi_t m_chi;
};

class SendPrepare : public Action {
public:
  SendPrepare(PlTerm Replica_id, PlTerm Req_digest, PlTerm View, PlTerm Seq_number);
  ~SendPrepare(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::SEND_PREPARE; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::SendPrepare>> BuildActives(const itcoin::FbftConfig& config,
                                                                         blockchain::Blockchain& blockchain,
                                                                         wallet::RoastWallet& wallet);

private:
  messages::Request m_request;
  uint32_t m_view;
  uint32_t m_seq_number;
};

class SendPrePrepare : public Action {
public:
  SendPrePrepare(blockchain::Blockchain& blockchain, PlTerm Replica_id, PlTerm Req_digest, PlTerm V,
                 PlTerm N);
  ~SendPrePrepare(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::SEND_PRE_PREPARE; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::SendPrePrepare>>
  BuildActives(const itcoin::FbftConfig& config, blockchain::Blockchain& blockchain,
               wallet::RoastWallet& wallet);

private:
  blockchain::Blockchain& m_blockchain;

  messages::Request m_request;
  uint32_t m_view;
  uint32_t m_seq_number;
};

class SendViewChange : public Action {
public:
  SendViewChange(uint32_t replica_id, uint32_t view) : Action(replica_id), m_view(view){};
  ~SendViewChange(){};

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::SEND_VIEW_CHANGE; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::SendViewChange>>
  BuildActives(const itcoin::FbftConfig& config, blockchain::Blockchain& blockchain,
               wallet::RoastWallet& wallet);

private:
  uint32_t m_view;
};

class RoastInit : public Action {
  messages::Request m_request;
  uint32_t m_view;
  uint32_t m_seq_number;

public:
  RoastInit(PlTerm Replica_id, PlTerm Req_digest, PlTerm V, PlTerm N);
  ~RoastInit();

  std::string identify() const;
  ACTION_TYPE type() const { return ACTION_TYPE::ROAST_INIT; }

  int effect() const;

  static std::vector<std::unique_ptr<actions::RoastInit>> BuildActives(const itcoin::FbftConfig& config,
                                                                       blockchain::Blockchain& blockchain,
                                                                       wallet::RoastWallet& wallet);
};

class RoastReceivePreSignature : public Action {
public:
  RoastReceivePreSignature(wallet::RoastWallet& wallet, uint32_t replica_id, messages::RoastPreSignature msg);
  ~RoastReceivePreSignature();

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::ROAST_RECEIVE_PRE_SIGNATURE; }

  int effect() const;

private:
  wallet::RoastWallet& m_wallet;
  messages::RoastPreSignature m_msg;
};

class RoastReceiveSignatureShare : public Action {
public:
  RoastReceiveSignatureShare(uint32_t replica_id, messages::RoastSignatureShare msg)
      : Action(replica_id), m_msg(msg){};
  ~RoastReceiveSignatureShare(){};

  std::string identify() const;
  std::optional<std::reference_wrapper<const messages::Message>> message() const {
    return std::optional<std::reference_wrapper<const messages::Message>>((const messages::Message&)m_msg);
  }
  ACTION_TYPE type() const { return ACTION_TYPE::ROAST_RECEIVE_SIGNATURE_SHARE; }

  int effect() const;

private:
  messages::RoastSignatureShare m_msg;
};

} // namespace actions
} // namespace fbft
} // namespace itcoin

#endif // ITCOIN_FBFT_ACTIONS_ACTIONS_H
