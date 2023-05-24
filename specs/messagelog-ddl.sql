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

CREATE TABLE peer_roles (
    roleId INTEGER CHECK(roleId IN (1, 2)) PRIMARY KEY,
    name STRING
);

----------

INSERT INTO peer_roles
    (roleId, name)
VALUES
    (1, "REPLICA"),
    (2, "CLIENT");

----------

CREATE TABLE msg_types (
    tag INTEGER CHECK(tag IN (1, 2, 3, 4, 5, 6, 7, 8)) PRIMARY KEY,
    name STRING
);

----------

INSERT INTO msg_types
    (tag, name)
VALUES
    (1, "REQUEST"),
    (2, "REPLY"),
    (3, "PREPREPARE"),
    (4, "PREPARE"),
    (5, "COMMIT"),
    (6, "CHECKPOINT"),
    (7, "VIEWCHANGE"),
    (8, "NEWVIEW");

----------

CREATE TABLE msg_log (
    _id INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
    senderRole INTEGER NOT NULL REFERENCES peer_roles(roleId) ON UPDATE RESTRICT ON DELETE RESTRICT,
    senderId UNSIGNED INTEGER NOT NULL,
    tag INTEGER NOT NULL REFERENCES msg_types(tag) ON UPDATE RESTRICT ON DELETE RESTRICT,

    -- Should we really have a dedicated "replicaId" field at pbft level or
    -- should we reuse senderId from transport?
    replicaId UNSIGNED INTEGER DEFAULT NULL CHECK(CASE
        -- When the message is in (REPLY, PREPARE, COMMIT, CHECKPOINT) replicaId
        -- must be not null.
        WHEN (tag IN (2, 4, 5, 6) AND replicaId IS NOT NULL) THEN TRUE

       -- When the message is in (REQUEST, PREPREPARE, VIEWCHANGE, NEWVIEW)
       -- replicaId must be null
        WHEN (tag NOT IN (2, 4, 5, 6) AND replicaId IS NULL) THEN TRUE

        -- everything else is not allowed
        ELSE FALSE
    END),

    seqNumber UNSIGNED INTEGER DEFAULT NULL CHECK(CASE
        -- PREPREPARE, PREPARE, COMMIT, CHECKPOINT and VIEWCHANGE are required
        -- to have a seqNumber
        WHEN (tag IN (3, 4, 5, 6, 7) AND seqNumber IS NOT NULL) THEN TRUE

        -- REQUEST, REPLY, NEWVIEW are required to have a NULL seqNumber
        WHEN (tag NOT IN (3, 4, 5, 6, 7) AND seqNumber IS NULL) THEN TRUE

        -- everything else is not allowed
        ELSE FALSE
    END),

    -- "view" is a SQL reserved word, but the parser seems smart enough to accept it
    view UNSIGNED INTEGER DEFAULT NULL CHECK(CASE
        -- REPLY, PREPREPARE, PREPARE and COMMIT are required to have a view
        WHEN (tag IN (2, 3, 4, 5) AND view IS NOT NULL) THEN TRUE

        -- REQUEST, VIEWCHANGE, NEWVIEW and CHECKPOINT are required to have a
        -- NULL view
        WHEN (tag NOT IN (2, 3, 4, 5) AND view IS NULL) THEN TRUE

        -- everything else is not allowed
        ELSE FALSE
    END),

    -- For PREPREPARE, PREPARE and COMMIT messages, this field stores the sha256
    -- of the on-the-wire representation of the RequestMessage to which this
    -- message is responding.
    --
    -- Please note that the field does not contain a textual representation of
    -- the sha256, but its binary form, and thus it has to be exactly 32 bytes
    -- long.
    requestDigest BLOB DEFAULT NULL CHECK(CASE
        -- PREPREPARE, PREPARE and COMMIT are required to have a non null
        -- requestDigest
        WHEN (
            tag IN (3, 4, 5) AND
            requestDigest IS NOT NULL
            AND length(requestDigest) = 32
        ) THEN TRUE

        -- REQUEST, REPLY, VIEWCHANGE, NEWVIEW and CHECKPOINT are required to
        --- have a NULL requestDigest
        WHEN (tag NOT IN (3, 4, 5) AND requestDigest IS NULL) THEN TRUE

        -- everything else is not allowed
        ELSE FALSE
    END),

    -- an empty buffer deserializes to a protobuf populated with default
    -- values, and vice versa: a protobuf only populated with default values
    -- may serialize to an empty buffer.
    --
    -- REMARK:
    --     Please note that - due to implementation details - an empty message
    --     is stored as an empty value with STRING instead of BLOB affinity. In
    --     sqlite3 command line syntax, the empty message is "" instead of x"".
    --     Non-empty messages are stored as true BLOBS.
    serializedPayload BLOB NOT NULL,

    -- the sha256() sum of serializedPayload.
    --
    -- The field value is automatically computed upon insertion using a User
    -- Defined Function.
    --
    -- Please note that the field does not contain a textual representation of
    -- the sha256, but its binary form, and thus it has to be exactly 32 bytes
    -- long. Consequently, lookups have to be done against a binary (and not
    -- textual) representation.
    messageDigest BLOB NOT NULL AS (sha256(serializedPayload)) STORED CHECK(
        length(messageDigest) = 32
    ),

    -- When a replica sends a REPLY to a REQUEST message, this field stores the
    -- on-the-wire reply that was sent, in order to be able to send it again if
    -- needed.
    --
    -- REMARK:
    --     Please note that - due to implementation details - an empty message
    --     is stored as an empty value with STRING instead of BLOB affinity. In
    --     sqlite3 command line syntax, the empty message is "" instead of x"".
    --     Non-empty messages are stored as true BLOBS.
    serializedReply BLOB DEFAULT NULL CHECK(CASE
        -- if the message is a Request, we are allowed to store the associated
        -- reply
        WHEN (tag = 1) THEN TRUE

        -- if the message is not a Request and we are not attempting to store
        -- a reply, that's fine too
        WHEN ((tag <> 1) AND (serializedReply IS NULL)) THEN TRUE

        -- if the message is not a request, then it makes no sense to attempt
        -- to store a reply: only null is allowed
        WHEN ((tag <> 1) AND (serializedReply IS NOT NULL)) THEN FALSE
    END)
);

----------

-- A serializedPayload must always be present and be unique for each message
-- type
CREATE UNIQUE INDEX "idx_msglog_tag_serialized_payload"
ON "msg_log" ("tag", "serializedPayload");

----------

-- There can be multiple rows where serializedReply is NULL. But if it is
-- not null, it has to be unique.
CREATE UNIQUE INDEX "idx_msglog_serialized_reply"
ON "msg_log" ("serializedReply")
WHERE serializedReply is NOT NULL;

----------

-- human readable view on the message log. Useful for debugging purposes
CREATE VIEW human_log AS
SELECT
    _id,
    peer_roles.name || '#' || senderId AS sender,
    msg_types.name as msg_type,
    replicaId,
    seqNumber,
    view,
    hex(requestDigest),
    hex(serializedPayload),
    hex(messageDigest),
    hex(serializedReply)
FROM
    msg_log
INNER JOIN
    peer_roles ON
    msg_log.senderRole = peer_roles.roleId
INNER JOIN
    msg_types ON
    msg_log.tag = msg_types.tag;
