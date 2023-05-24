#ifndef ITCOIN_PBFT_MESSAGES_MESSAGES_H
#define ITCOIN_PBFT_MESSAGES_MESSAGES_H

#include <string>
#include <vector>

#include <json/json.h>

#include <SWI-cpp.h>

#include "../../blockchain/blockchain.h"

namespace itcoin { namespace wallet {
  class Wallet;
}}

namespace wallet=itcoin::wallet;

namespace itcoin {
namespace pbft {
namespace messages {

enum NODE_TYPE : unsigned int {
  REPLICA = 0,
  CLIENT = 1,
};

const std::string NODE_TYPE_AS_STRING[] = { "REPLICA", "CLIENT" };

enum MSG_TYPE : unsigned int {
  BLOCK = 0,
  COMMIT = 1,
  NEW_VIEW = 2,
  PREPARE = 3,
  PRE_PREPARE = 4,
  REQUEST = 5,
  ROAST_PRE_SIGNATURE = 6,
  ROAST_SIGNATURE_SHARE = 7,
  VIEW_CHANGE = 8,
};

const std::string MSG_TYPE_AS_STRING[] = {
  "BLOCK",
  "COMMIT",
  "NEW_VIEW",
  "PREPARE",
  "PRE_PREPARE",
  "REQUEST",
  "ROAST_PRE_SIGNATURE",
  "ROAST_SIGNATURE_SHARE",
  "VIEW_CHANGE",
};

// Type definitions for messages

typedef std::tuple<uint32_t, std::string, std::string, uint32_t> view_change_pre_prepared_elem_t;
typedef std::vector<view_change_pre_prepared_elem_t> view_change_pre_prepared_t;

typedef std::tuple<uint32_t, std::string, uint32_t> view_change_prepared_elem_t;
typedef std::vector<view_change_prepared_elem_t> view_change_prepared_t;

typedef std::tuple<uint32_t, std::string> new_view_nu_elem_t;
typedef std::vector<new_view_nu_elem_t> new_view_nu_t;

typedef std::tuple<uint32_t, std::string, std::string> new_view_chi_elem_t;
typedef std::vector<new_view_chi_elem_t> new_view_chi_t;

// Abstract class

class Message {
  public:
    Message(NODE_TYPE sender_role, uint32_t sender_id);
    Message(NODE_TYPE sender_role, PlTerm Sender_id);
    virtual ~Message();

    // Getters
    virtual std::unique_ptr<Message> clone() = 0;
    virtual const std::string digest() const;
    virtual std::string identify() const = 0;
    std::string name() const;
    virtual std::optional<uint32_t> seq_number_as_opt() const;
    std::string signature() const;
    uint32_t sender_id() const {return m_sender_id;}
    virtual MSG_TYPE type() const = 0;

    // Setters
    void set_signature(std::string signature_hex);

    // Operations
    virtual void Sign(const wallet::Wallet& wallet);
    virtual bool VerifySignatures(const wallet::Wallet& wallet);

    // Operators
    friend std::ostream& operator<<(std::ostream& Str, const Message& action);
    bool operator==(const Message& other) const;

    // Serialization
    virtual std::string ToBinBuffer() const; // Should be = 0;

    // TODO: ritornare direttamente uno unique_ptr, eventualmente nullptr
    static std::optional<std::unique_ptr<messages::Message>> BuildFromBinBuffer(const std::string& bin_buffer);

  protected:
    Message(const Json::Value& root);

    NODE_TYPE m_sender_role;
    uint32_t m_sender_id;
    std::string m_signature;

    virtual bool equals(const Message& other) const;
    std::string FinalizeJsonRoot(Json::Value& root) const;
};

class Request : public Message {
  public:
    Request();
    // TODO: the timestamp type should become an uin32_t like the nTime field in block.h
    Request(uint32_t genesis_block_timestamp, uint32_t target_block_time, uint32_t timestamp);
    ~Request();

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    std::string identify() const;
    uint32_t timestamp() const { return m_timestamp; }
    uint32_t height() const;
    MSG_TYPE type() const { return MSG_TYPE::REQUEST; }

    // Finders
    // TODO, replace with one optional
    static messages::Request FindByDigest(uint32_t replica_id, std::string req_digest);
    static bool TryFindByDigest(uint32_t replica_id, const std::string req_digest, messages::Request& out_request);

  private:
    uint32_t m_genesis_block_timestamp;
    uint32_t m_target_block_time;
    uint32_t m_timestamp;

    bool equals(const Message& other) const;
};

class PrePrepare : public Message {
  public:
    PrePrepare(uint32_t sender_id,
      uint32_t view, uint32_t seq_number, std::string req_digest,
      CBlock proposed_block);
    PrePrepare(PlTerm Sender_id, PlTerm V, PlTerm N, PlTerm Req_digest, PlTerm Proposed_block);
    PrePrepare(const Json::Value& root);
    ~PrePrepare();

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    std::string identify() const;
    uint32_t view() const { return m_view; }
    uint32_t seq_number() const { return m_seq_number; }
    std::optional<uint32_t> seq_number_as_opt() const { return m_seq_number; }
    std::string req_digest() const { return m_req_digest; }
    MSG_TYPE type() const { return MSG_TYPE::PRE_PREPARE; }
    const CBlock& proposed_block() const { return m_proposed_block; }
    std::string proposed_block_hex() const;

    // Builders
    static std::vector<std::unique_ptr<messages::PrePrepare>> BuildToBeSent(uint32_t replica_id);

    // Finders
    static messages::PrePrepare FindByV_N_Req(uint32_t replica_id, uint32_t v, uint32_t n, std::string req_digest);

    // Serialization
    std::string ToBinBuffer() const;

  private:
    uint32_t m_view;
    uint32_t m_seq_number;
    std::string m_req_digest;
    itcoin::blockchain::HexSerializableCBlock m_proposed_block;

    bool equals(const Message& other) const;
    void set_proposed_block(std::string proposed_block_hex);
};

class Prepare : public Message {
  public:
    Prepare(uint32_t sender_id,
      uint32_t view, uint32_t seq_number, std::string req_digest);
    Prepare(PlTerm Sender_id, PlTerm V, PlTerm N, PlTerm Req_digest);
    Prepare(const Json::Value& root);
    ~Prepare();

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    std::string identify() const;
    std::string req_digest() const { return m_req_digest; }
    uint32_t seq_number() const { return m_seq_number; }
    std::optional<uint32_t> seq_number_as_opt() const { return m_seq_number; }
    MSG_TYPE type() const { return MSG_TYPE::PREPARE; }
    uint32_t view() const { return m_view; }

    // Builders
    static std::vector<std::unique_ptr<messages::Prepare>> BuildToBeSent(uint32_t replica_id);

    // Serialization
    std::string ToBinBuffer() const;

  private:
    uint32_t m_view;
    uint32_t m_seq_number;
    std::string m_req_digest;

    bool equals(const Message& other) const;
};

class Commit : public Message {
  public:
    Commit(uint32_t sender_id, uint32_t view, uint32_t seq_number, std::string block_signature);
    Commit(PlTerm Sender_id, PlTerm V, PlTerm N, PlTerm Block_signature);
    Commit(const Json::Value& root);
    ~Commit();

    // Getters
    std::unique_ptr<Message> clone();
    const std::string block_signature() const { return m_block_signature; }
    const std::string digest() const;
    std::string identify() const;
    uint32_t seq_number() const { return m_seq_number; }
    std::optional<uint32_t> seq_number_as_opt() const { return m_seq_number; }
    MSG_TYPE type() const { return MSG_TYPE::COMMIT; }
    uint32_t view() const { return m_view; }

    // Finders
    static std::vector<messages::Commit> FindByV_N(uint32_t replica_id, uint32_t v, uint32_t n);

    // Builders
    static std::vector<std::unique_ptr<messages::Commit>> BuildToBeSent(uint32_t replica_id);

    // Serialization
    std::string ToBinBuffer() const;

  private:
    uint32_t m_view;
    uint32_t m_seq_number;
    std::string m_block_signature;

    bool equals(const Message& other) const;
    void set_block_signature(std::string block_signature_hex);
};

class Block : public Message {
  public:
    Block(uint32_t block_height, uint32_t block_time, std::string block_hash);
    ~Block();

    // Getters
    std::unique_ptr<Message> clone();
    uint32_t block_time() const { return m_block_time; }
    std::string block_hash() const { return m_block_hash; };
    uint32_t block_height() const { return m_block_height; }
    std::string identify() const;
    MSG_TYPE type() const { return MSG_TYPE::BLOCK; }

  private:
    uint32_t m_block_height;
    std::string m_block_hash;
    uint32_t m_block_time;

    bool equals(const Message& other) const;
};

class ViewChange : public Message {
  public:
    ViewChange(uint32_t sender_id,
      uint32_t view, uint32_t hi,
      std::string c,
      view_change_prepared_t pi,
      view_change_pre_prepared_t qi);
    ViewChange(PlTerm Sender_id,
      PlTerm V, PlTerm Hi, PlTerm C, PlTerm Pi, PlTerm Qi);
    ViewChange(const Json::Value& root);
    ~ViewChange();

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    uint32_t view() const { return m_view; }
    uint32_t hi() const { return m_hi; }
    std::string identify() const;
    const std::string c() const { return m_c; }
    const view_change_prepared_t& pi() const { return m_pi; }
    PlTerm pi_as_plterm() const;
    const view_change_pre_prepared_t& qi() const { return m_qi; }
    PlTerm qi_as_plterm() const;
    MSG_TYPE type() const { return MSG_TYPE::VIEW_CHANGE; }

    // Builders
    static std::vector<std::unique_ptr<messages::ViewChange>> BuildToBeSent(uint32_t replica_id);

    // Finders
    static messages::ViewChange FindByDigest(uint32_t replica_id, uint32_t sender_id, std::string digest);

    // Serialization
    std::string ToBinBuffer() const;

  private:
    uint32_t m_view;
    uint32_t m_hi;
    std::string m_c;
    view_change_prepared_t m_pi;
    view_change_pre_prepared_t m_qi;

    bool equals(const Message& other) const;
};

class NewView: public Message {
  public:
    NewView(uint32_t sender_id,
      uint32_t view,
      std::vector<ViewChange> vc_messages,
      std::vector<PrePrepare> ppp_messages
    );
    NewView(PlTerm Sender_id,
      PlTerm V, PlTerm Nu, PlTerm Chi
    );
    NewView(const Json::Value& root);
    ~NewView();

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    std::string identify() const;
    const std::vector<ViewChange>& view_changes() const { return m_vc_messages; };
    new_view_nu_t nu() const;
    const std::vector<PrePrepare>& pre_prepares() const { return m_ppp_messages; };
    new_view_chi_t chi() const;
    MSG_TYPE type() const { return MSG_TYPE::NEW_VIEW; }
    uint32_t view() const { return m_view; }

    // Builders
    static std::vector<std::unique_ptr<messages::NewView>> BuildToBeSent(uint32_t replica_id);
    static new_view_nu_t nu_from_plterm(PlTerm Nu);
    static PlTerm nu_as_plterm(new_view_nu_t nu);
    static new_view_chi_t chi_from_plterm(PlTerm Chi);
    static PlTerm chi_as_plterm(new_view_chi_t chi);

    // Operations
    void Sign(const wallet::Wallet& wallet);
    bool VerifySignatures(const wallet::Wallet& wallet);

    // Serialization
    std::string ToBinBuffer() const;

  private:
    uint32_t m_view;
    std::vector<ViewChange> m_vc_messages;
    std::vector<PrePrepare> m_ppp_messages;

    bool equals(const Message& other) const;
};

class RoastPreSignature : public Message {
  public:
    RoastPreSignature(uint32_t sender_id,
      std::vector<uint32_t> signers, std::string pre_signature);
    RoastPreSignature(PlTerm Sender_id,
      PlTerm Signers, PlTerm Pre_signature);
    RoastPreSignature(const Json::Value& root);
    ~RoastPreSignature();

    // Builders
    static std::vector<std::unique_ptr<messages::RoastPreSignature>> BuildToBeSent(uint32_t replica_id);

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    std::string identify() const;
    std::string pre_signature() const;
    std::vector<uint32_t> signers() const;
    PlTerm signers_as_plterm() const;
    MSG_TYPE type() const { return MSG_TYPE::ROAST_PRE_SIGNATURE; }

    // Serialization
    std::string ToBinBuffer() const;

  private:
    std::vector<uint32_t> m_signers;
    std::string m_pre_signature;

    bool equals(const Message& other) const;
};

class RoastSignatureShare : public Message {
  public:
    RoastSignatureShare(uint32_t sender_id,
      std::string signature_share, std::string next_pre_signature_share);
    RoastSignatureShare(PlTerm Sender_id,
      PlTerm Signature_share, PlTerm Next_pre_signature_share);
    RoastSignatureShare(const Json::Value& root);
    ~RoastSignatureShare();

    // Builders
    static std::vector<std::unique_ptr<messages::RoastSignatureShare>> BuildToBeSent(uint32_t replica_id);

    // Getters
    std::unique_ptr<Message> clone();
    const std::string digest() const;
    std::string identify() const;
    std::string signature_share() const;
    std::string next_pre_signature_share() const;
    MSG_TYPE type() const { return MSG_TYPE::ROAST_SIGNATURE_SHARE; }

    // Serialization
    std::string ToBinBuffer() const;

  private:
    std::string m_signature_share;
    std::string m_next_pre_signature_share;

    bool equals(const Message& other) const;
};

}
}
}

#endif // ITCOIN_PBFT_MESSAGES_MESSAGES_H
