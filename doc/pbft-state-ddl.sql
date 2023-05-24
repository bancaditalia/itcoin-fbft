-- This file contains the schema definition for the replica message log, as used
-- by class itcoin::pbft::ReplicaState.
--
-- It is a standard SQL file, but there is a quirk: the SQLite3 wrapper library
-- is only able to execute a single statement at a time, so ReplicaState has to
-- execute it piecewise.
--
-- Please, separate each SQL statement with a separator. A valid separator is a
-- line in which the first five non blank characters are 10 dashes, like this:
-- "----------".

-- State scalar variables
CREATE TABLE state_scalar_vars (
    _id SMALLINT PRIMARY KEY NOT NULL CHECK (_id=1),

    -- PBFT config variables documented pag. 452
    --  the current value of the replica’s copy of the state machine
    stateVal BYTEA NOT NULL CHECK( length(stateVal) = 32 ),

    -- Replicas also maintain the current view number v, and a flag that
    -- indicates whether the view change into v is complete
    view INTEGER NOT NULL,
    viewActive BOOLEAN NOT NULL,

    -- the sequence number of the last request executed
    lastExec INTEGER NOT NULL,

    -- the last sequence number they picked for a request.
    seqNumber INTEGER NOT NULL

    -- low watermark defined as the minimum n that is in checkpoints
    -- Use GET_LOW_WATERMARK query to get the low watermark
    -- lowWatermark INTEGER NOT NULL
);

----------

INSERT INTO state_scalar_vars
    (_id, stateVal, view, viewActive, lastExec, seqNumber)
VALUES
    (1, X'0000000000000000000000000000000000000000000000000000000000000000', 0, true, 0, 0);

----------

-- the last reply sent to each client
-- and the timestamps in those replies
-- This is a function from the client space to the output space
-- and integer space
CREATE TABLE state_last_reply (
    clientId INTEGER PRIMARY KEY,

    serializedReply BYTEA NOT NULL,

    timestamp INTEGER NOT NULL
);

----------

-- There is also a set of checkpoints , whose elements contain
-- not only a snapshot of the state but also of lastRep and lastRepT.
-- S x (U -> O') x (U -> N) initially < 0, <stateVal, lastRep, lastRepT> >
CREATE TABLE state_checkpoints (
    seqNumber INTEGER NOT NULL,

    stateVal BYTEA NOT NULL CHECK( length(stateVal) = 32 ),

    clientId INTEGER,

    serializedReply BYTEA DEFAULT NULL,

    timestamp INTEGER DEFAULT NULL
);

----------

-- Pi and Qi are used during view changes as explained in Section 4.5.
-- Pi is a subset of N^3
CREATE TABLE state_viewchange_p (
    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    requestDigest BYTEA NOT NULL CHECK( length(requestDigest) = 32 )
);

----------

-- Qi is a subset of N^3
CREATE TABLE state_viewchange_q (
    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    requestDigest BYTEA NOT NULL CHECK( length(requestDigest) = 32 )
);

----------

-- buffers messages that are about to be sent.
CREATE TABLE out_msg_buffer (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    serializedPayload BYTEA NOT NULL,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

CREATE TABLE msg_log_request (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    -- operation is omitted, since the FSM has only one operation

    -- "timestamp" is a SQL reserved word, but the parser seems smart enough to accept it
    timestamp INTEGER NOT NULL,

    clientId INTEGER NOT NULL,

    -- an empty buffer deserializes to a protobuf populated with default
    -- values, and vice versa: a protobuf only populated with default values
    -- may serialize to an empty buffer.
    --
    -- REMARK:
    --     Please note that - due to implementation details - an empty message
    --     is stored as an empty value with STRING instead of BLOB affinity. In
    --     sqlite3 command line syntax, the empty message is "" instead of x"".
    --     Non-empty messages are stored as true BLOBS.
    serializedPayload BYTEA NOT NULL UNIQUE,

    -- the sha256() sum of serializedPayload.
    --
    -- The field value is automatically computed upon insertion using a User
    -- Defined Function.
    --
    -- Please note that the field does not contain a textual representation of
    -- the sha256, but its binary form, and thus it has to be exactly 32 bytes
    -- long. Consequently, lookups have to be done against a binary (and not
    -- textual) representation.
    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )

);

----------

CREATE TABLE msg_log_reply (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    senderId INTEGER NOT NULL,

    -- "view" is a SQL reserved word, but the parser seems smart enough to accept it
    view INTEGER NOT NULL,

    -- "timestamp" is a SQL reserved word, but the parser seems smart enough to accept it
    timestamp INTEGER NOT NULL,

    clientId INTEGER NOT NULL,

    -- When a replica sends a REPLY to a REQUEST message, this field stores the
    -- on-the-wire reply that was sent, in order to be able to send it again if
    -- needed.
    --
    -- REMARK:
    --     Please note that - due to implementation details - an empty message
    --     is stored as an empty value with STRING instead of BLOB affinity. In
    --     sqlite3 command line syntax, the empty message is "" instead of x"".
    --     Non-empty messages are stored as true BLOBS.
    serializedReply BYTEA NOT NULL,

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

CREATE TABLE msg_log_pre_prepare (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    -- For PREPREPARE and PREPARE messages, this field stores the sha256
    -- of the on-the-wire representation of the RequestMessage to which this
    -- message is responding.
    --
    -- Please note that the field does not contain a textual representation of
    -- the sha256, but its binary form, and thus it has to be exactly 32 bytes
    -- long.
    requestDigest BYTEA NOT NULL CHECK(length(requestDigest) = 32),

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

CREATE TABLE msg_log_prepare (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    senderId INTEGER NOT NULL,

    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    requestDigest BYTEA NOT NULL CHECK(length(requestDigest) = 32),

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

CREATE TABLE msg_log_commit (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    senderId INTEGER NOT NULL,

    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

CREATE TABLE msg_log_checkpoint (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    senderId INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    stateDigest BYTEA NOT NULL CHECK( length(stateDigest) = 32 ),

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

CREATE TABLE msg_log_viewchange (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    senderId INTEGER NOT NULL,

    view INTEGER NOT NULL,

    lowWatermark INTEGER NOT NULL,

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

-- newview C
CREATE TABLE msg_log_viewchange_c (

    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    viewchangeId INTEGER NOT NULL REFERENCES msg_log_viewchange(_id) ON UPDATE RESTRICT ON DELETE RESTRICT,

    seqNumber INTEGER NOT NULL,

    stateDigest BYTEA NOT NULL CHECK(length(stateDigest) = 32)
);

----------

-- newview Pi
CREATE TABLE msg_log_viewchange_p (

    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    viewchangeId INTEGER NOT NULL REFERENCES msg_log_viewchange(_id) ON UPDATE RESTRICT ON DELETE RESTRICT,

    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    requestDigest BYTEA NOT NULL CHECK( length(requestDigest) = 32 )
);

----------

-- newview Qi
CREATE TABLE msg_log_viewchange_q (

    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    viewchangeId INTEGER NOT NULL REFERENCES msg_log_viewchange(_id) ON UPDATE RESTRICT ON DELETE RESTRICT,

    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    requestDigest BYTEA NOT NULL CHECK( length(requestDigest) = 32 )

);

----------

CREATE TABLE msg_log_newview (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    view INTEGER NOT NULL,

    seqNumber INTEGER NOT NULL,

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);

----------

-- newview V
CREATE TABLE msg_log_newview_v (

    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    newviewId INTEGER NOT NULL REFERENCES msg_log_newview(_id) ON UPDATE RESTRICT ON DELETE RESTRICT,

    viewChangeSender INTEGER NOT NULL,

    viewChangeDigest BYTEA NOT NULL CHECK( length(viewChangeDigest) = 32 )
);

----------

-- This is newview X, documented at page 412, 456
CREATE TABLE msg_log_newview_x (

    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    newviewId INTEGER NOT NULL REFERENCES msg_log_newview(_id) ON UPDATE RESTRICT ON DELETE RESTRICT,

    seqNumber INTEGER NOT NULL,

    stateDigest BYTEA NOT NULL CHECK(length(stateDigest) = 32)
);

----------

-- Documented at page 412
CREATE TABLE msg_log_viewchangeack (
    _id INTEGER PRIMARY KEY AUTOINCREMENT,

    senderId INTEGER NOT NULL,

    view INTEGER NOT NULL,

    -- j
    viewChangeSender INTEGER NOT NULL,

    -- d
    viewChangeDigest BYTEA NOT NULL CHECK( length(messageDigest) = 32 ),

    serializedPayload BYTEA NOT NULL UNIQUE,

    messageDigest BYTEA NOT NULL AS (sha256(serializedPayload)) STORED CHECK( length(messageDigest) = 32 )
);
