# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

import csv
import dataclasses
import datetime
import os
import re
from abc import abstractmethod, ABC
from collections import defaultdict
from enum import Enum
from glob import glob
from multiprocessing import Pool
from operator import attrgetter
from os.path import join
from pathlib import Path
from re import findall, search
from statistics import mean, stdev
from typing import List, Mapping, Dict, Protocol, TypeVar, Tuple, Optional, Set, Type, cast

import numpy as np

from benchmark_framework.config import BenchParameters, DEFAULT_BENCH_PARAMETERS_FILENAME
from benchmark_framework.paths import PathMaker
from benchmark_framework.utils import PathLike, NodeType


class ParseError(Exception):
    pass


# datetime.timestamp returns float
PosixTimestamp = float
Height = int
ReplicaId = int
View = int
SeqNumber = int

UNDEFINED_TIMESTAMP = -1

RESULT_HEADERS = ['Nodes',
    'Faults',
    'Target Block Time',
    'Rate',
    'Genesis hours in the past',
    'Tx size',
    'Execution time',
    'Warm-up time',
    'Consensus blocks/s',
    'Consensus block latency',
    'Consensus block latency std',
    'Latency at fault height',
    'Time for FBFT',
    'Time for ROAST',
    'Time for Bitcoin',
    ]


class LogEventThatHasHeightAndTimestamp(Protocol):

    block_height: Height
    log_timestamp: PosixTimestamp


LogEventType = TypeVar("LogEventType", bound=LogEventThatHasHeightAndTimestamp)


@dataclasses.dataclass(frozen=True)
class LogEvent(ABC):
    log_timestamp: PosixTimestamp

    # not actually needed as we know to which node a log belongs to,
    # but this might be useful in other places of the log parsing code
    replica_id: ReplicaId

    @classmethod
    @abstractmethod
    def from_log_row(cls, log_row: str) -> "LogEvent":
        """Parse the log event instance from a log row."""
        raise NotImplementedError


@dataclasses.dataclass(frozen=True)
class ReceiveRequestsLog(LogEvent):
    """Log data for a receive request."""
    block_timestamp: PosixTimestamp
    block_height: Height

    @property
    def true_request_time(self) -> PosixTimestamp:
        """
        Get the true request time.

        The true request time is the maximum between log timestamp and block timestamp
        """
        return max(self.block_timestamp, self.log_timestamp)

    @classmethod
    def from_log_row(cls, log_row: str) -> "ReceiveRequestsLog":
        """Parse the log event instance from a log row."""
        search_obj = re.search(r'\[(.*?)] .* applying <RECEIVE_REQUEST, T=([0-9]+), H=([0-9]+), R=([0-9]+)', log_row)
        log_timestamp_str = search_obj.group(1)
        request_posix_timestamp_str = search_obj.group(2)
        height_str = search_obj.group(3)
        replica_id_str = search_obj.group(4)
        log_event_obj = ReceiveRequestsLog(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(request_posix_timestamp_str),
            int(height_str)
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class ReceiveBlockLog(LogEvent):
    """Log data for a receive block event."""

    block_height: Height

    @classmethod
    def from_log_row(cls, log_row: str) -> "ReceiveBlockLog":
        search_obj = re.search(r'\[(.*?)] .* applying <RECEIVE_BLOCK, H=([0-9]+), R=([0-9]+)', log_row)
        log_timestamp_str = search_obj.group(1)
        height_str = search_obj.group(2)
        replica_id_str = search_obj.group(3)
        log_event_obj = ReceiveBlockLog(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(height_str)
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class StartTestBlockValidity(LogEvent):
    """Log data for a start of the test block validity request."""

    block_height: Height
    block_size: int
    block_hash: str

    @classmethod
    def from_log_row(cls, log_row: str) -> "StartTestBlockValidity":
        search_obj = re.search("\[(.*?)] .* R([0-9]+) BitcoinBlockchain::TestBlockValidity invoking for candidate block at height ([0-9]+), blocksize ([0-9]+) bytes, block hash: ([abcdef0-9]+)", log_row)
        log_timestamp_str = search_obj.group(1)
        replica_id_str = search_obj.group(2)
        height_str = search_obj.group(3)
        block_size_str = search_obj.group(4)
        block_hash_str = search_obj.group(5)
        log_event_obj = StartTestBlockValidity(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(height_str),
            int(block_size_str),
            block_hash_str
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class EndTestBlockValidity(LogEvent):
    """Log data for the end of a test block validity request."""

    block_height: Height
    block_hash: str
    result: str

    @classmethod
    def from_log_row(cls, log_row: str) -> "EndTestBlockValidity":
        search_obj = re.search(r"\[(.*?)] .* R([0-9]+) BitcoinBlockchain::TestBlockValidity for candidate block at height ([0-9]+), block hash: ([abcdef0-9]+). Result = (.*) \(null means ok\)", log_row)
        log_timestamp_str = search_obj.group(1)
        replica_id_str = search_obj.group(2)
        height_str = search_obj.group(3)
        block_hash_str = search_obj.group(4)
        result = search_obj.group(5)
        log_event_obj = EndTestBlockValidity(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(height_str),
            block_hash_str,
            result
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class StartSubmitBlock(LogEvent):
    """Log data for the start of a submit block request."""

    block_height: Height
    block_size: int
    block_hash: str

    @classmethod
    def from_log_row(cls, log_row: str) -> "StartSubmitBlock":
        search_obj = re.search("\[(.*?)] .* R([0-9]+) BitcoinBlockchain::SubmitBlock submitting block at height ([0-9]+) block size: ([0-9]+) bytes, block hash: ([abcdef0-9]+)", log_row)
        log_timestamp_str = search_obj.group(1)
        replica_id_str = search_obj.group(2)
        height_str = search_obj.group(3)
        block_size_str = search_obj.group(4)
        block_hash_str = search_obj.group(5)
        log_event_obj = StartSubmitBlock(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(height_str),
            int(block_size_str),
            block_hash_str
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class EndSubmitBlock(LogEvent):
    """Log data for a start of the test block validity request."""

    block_height: Height
    block_hash: str
    result: str

    @classmethod
    def from_log_row(cls, log_row: str) -> "EndSubmitBlock":
        search_obj = re.search(r"\[(.*?)] .* R([0-9]+) BitcoinBlockchain::SubmitBlock for block at height ([0-9]+), block hash: ([abcdef0-9]+). Result = (.*) \(null means ok\)", log_row)
        log_timestamp_str = search_obj.group(1)
        replica_id_str = search_obj.group(2)
        height_str = search_obj.group(3)
        block_hash_str = search_obj.group(4)
        result = search_obj.group(5)
        log_event_obj = EndSubmitBlock(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(height_str),
            block_hash_str,
            result
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class PbftReplicaLogEvent(LogEvent, ABC):
    block_timestamp: PosixTimestamp
    block_height: Height
    view: View
    seq_number: SeqNumber

    @classmethod
    @abstractmethod
    def ACTION_NAME(cls):
        raise NotImplementedError

    @classmethod
    def from_log_row(cls, log_row: str) -> "PbftReplicaLogEvent":
        """Parse the log event instance from a log row."""
        search_obj = re.search(fr"\[(.*?)] .* applying <{cls.ACTION_NAME()}, Request=\(H=([0-9]+), T=([0-9]+)\), V=([0-9]+), N=([0-9]+), R=([0-9]+)>", log_row)
        log_timestamp_str = search_obj.group(1)
        height_str = search_obj.group(2)
        request_posix_timestamp_str = search_obj.group(3)
        view_str = search_obj.group(4)
        seq_number_str = search_obj.group(5)
        replica_id_str = search_obj.group(6)
        log_event_obj = ExecuteActionLog(
            LogParser.to_posix(log_timestamp_str),
            int(replica_id_str),
            int(request_posix_timestamp_str),
            int(height_str),
            int(view_str),
            int(seq_number_str)
        )
        return log_event_obj


@dataclasses.dataclass(frozen=True)
class SendPrePrepareLog(PbftReplicaLogEvent):
    """Log data for a SEND_PRE_PREPARE action."""

    @classmethod
    def ACTION_NAME(cls):
        return "SEND_PRE_PREPARE"


@dataclasses.dataclass(frozen=True)
class ExecuteActionLog(PbftReplicaLogEvent):
    """Log data for an execute action."""

    @classmethod
    def ACTION_NAME(cls):
        return "EXECUTE"


@dataclasses.dataclass(frozen=True)
class RoastInitLog(PbftReplicaLogEvent):
    """Log data for a ROAST_INIT action."""

    @classmethod
    def ACTION_NAME(cls):
        return "ROAST_INIT"


class TimeIntervalType(Enum):
    FBFT = "fbft"
    ROAST = "roast"
    BITCOIN = "bitcoin"


@dataclasses.dataclass(frozen=True)
class TimeInterval:
    """A data structure to memorize """
    start_time: PosixTimestamp
    end_time: PosixTimestamp
    interval_type: TimeIntervalType

    @property
    def diff(self) -> PosixTimestamp:
        return self.end_time - self.start_time

    def __post_init__(self):
        assert self.start_time < self.end_time


class TimeIntervals:
    """A collection of time intervals by block height."""

    def __init__(self, intervals_by_height: Dict[Height, Dict[TimeIntervalType, TimeInterval]]) -> None:
        self._intervals_by_height = intervals_by_height

    @property
    def intervals_by_height(self) -> Dict[Height, Dict[TimeIntervalType, TimeInterval]]:
        """Get the intervals-by-height dictionary."""
        return self._intervals_by_height

    def get_cumulative_time_by_interval_type(self, not_before: PosixTimestamp = 0) -> Dict[TimeIntervalType, float]:
        """
        Sum the time spent for each type of interval.

        :param not_before: don't return events before this time (default: allow everything)
        :return: a dictionary (interval type -> time spent in those intervals)
        """
        # TODO split the two parts
        diffs_by_height: Dict[Height, Dict[TimeIntervalType, float]] = {}
        for height, intervals in self.intervals_by_height.items():
            self._validate_keys(set(intervals.keys()), height)
            fbft_interval = intervals[TimeIntervalType.FBFT]
            roast_interval = intervals[TimeIntervalType.ROAST]
            submitblock_interval = intervals[TimeIntervalType.BITCOIN]
            if fbft_interval.start_time < not_before:
                continue
            diffs: Dict[TimeIntervalType, float] = {
                TimeIntervalType.FBFT: fbft_interval.diff,
                TimeIntervalType.ROAST: roast_interval.diff,
                TimeIntervalType.BITCOIN: submitblock_interval.diff
            }
            diffs_by_height[height] = diffs

        sums: Dict[TimeIntervalType, float] = defaultdict(lambda: 0.0)
        for height, diffs in diffs_by_height.items():
            for interval_type, diff in diffs.items():
                sums[interval_type] += diff
        return sums

    @property
    def nb_heights(self) -> int:
        return len(self.intervals_by_height)

    def _validate_keys(self, keys: Set[TimeIntervalType], height: Height):
        assert keys == set(TimeIntervalType), f"missing intervals of type {keys.difference(set(TimeIntervalType))} for height {height}"

    def get_time_spent_summary(self, not_before: PosixTimestamp = 0) -> str:
        time_by_type = self.get_cumulative_time_by_interval_type(not_before)
        time_spent_for_fbft = time_by_type[TimeIntervalType.FBFT]
        time_spent_for_roast = time_by_type[TimeIntervalType.ROAST]
        time_spent_for_bitcoin = time_by_type[TimeIntervalType.BITCOIN]
        total_time_spent = time_spent_for_fbft + time_spent_for_roast + time_spent_for_bitcoin
        return \
            f' Time spent for FBFT:                {round(time_spent_for_fbft * 1000):,} ms ({round(time_spent_for_fbft / total_time_spent * 100, ndigits=2)} %)\n' \
            f' Time spent for ROAST:               {round(time_spent_for_roast * 1000):,} ms ({round(time_spent_for_roast / total_time_spent * 100, ndigits=2)} %)\n' \
            f' Time spent for Bitcoin:             {round(time_spent_for_bitcoin * 1000):,} ms ({round(time_spent_for_bitcoin / total_time_spent * 100, ndigits=2)} %)\n'


@dataclasses.dataclass
class NodeLog:
    """Log data for a single node."""
    requests_by_height: Mapping[Height, ReceiveRequestsLog]
    executes_by_height: Mapping[Height, ExecuteActionLog]
    fault_height: int

    # parsed configuration
    config: Mapping

    # the raw log
    raw_log: str

    def __init__(self, log: str, config: Dict) -> None:
        """Initialize the NodeLog dataclass."""

        self._raw_log = log

        self._check_killed(self._raw_log)
        self._events_by_type_by_height = self._parse_logs(log)
        self._fault_height = self._parse_fault_height(self._raw_log, self.executes_by_height)

        self._config = config

    @property
    def raw_log(self) -> str:
        return self._raw_log

    @property
    def config(self) -> Mapping:
        return self._config

    @property
    def fault_height(self) -> int:
        return self._fault_height

    @property
    def events_by_type_by_height(self) -> Mapping[Type[LogEvent], Mapping[Height, LogEvent]]:
        return self._events_by_type_by_height

    @property
    def requests_by_height(self) -> Mapping[Height, ReceiveRequestsLog]:
        return self._events_by_type_by_height[ReceiveRequestsLog]

    @property
    def executes_by_height(self) -> Mapping[Height, ExecuteActionLog]:
        # there might be no executes if node was never the primary
        return self._events_by_type_by_height.get(ExecuteActionLog, {})

    @property
    def nb_executes(self) -> int:
        return len(self.executes_by_height)

    def _check_killed(self, log: str) -> None:
        tmp = search(r'.* \[ERROR\] Killed replica R([0-9]+) at view V=([0-9]+)', log)
        if tmp is not None:
            raise ParseError(f'Node R{tmp.group(1)} killed by mistake at view V={tmp.group(2)}')

    def _log_events_by_height(self, log_events: List[LogEventType]) -> Dict[Height, LogEventType]:
        merged = {}
        for log_event in log_events:
            block_height = log_event.block_height
            # TODO: temporary sanity check: be sure we have one log event for block height
            #       this might be relaxed to keep the earliest timestamp
            if block_height in merged:
                raise ValueError("more than one log event of the same type with the same block height")
            merged[block_height] = log_event
        return merged

    def _parse_logs(self, log: str):
        """
        Parse log (in one pass).

        :returns requests by height, executes by heights, fault height
        """

        # we scan the file in one-pass, this requires a bit of effort
        # in the preparation of the scanning loop
        events_by_type_by_height = {}
        # from row pattern filter to (log event class, list where to append it)
        patterns = {
            "applying <RECEIVE_REQUEST": ReceiveRequestsLog,
            "applying <EXECUTE": ExecuteActionLog,
            "applying <RECEIVE_BLOCK": ReceiveBlockLog,
            "applying <SEND_PRE_PREPARE": SendPrePrepareLog,
            "applying <ROAST_INIT": RoastInitLog,
            "BitcoinBlockchain::SubmitBlock submitting": StartSubmitBlock,
            "BitcoinBlockchain::SubmitBlock for block at height": EndSubmitBlock,
        }
        regex = re.compile(fr"{'|'.join(patterns.keys())}")
        for row in log.splitlines():
            match_result = regex.search(row)
            if match_result is None:
                continue
            event_log_cls = patterns[match_result.group(0)]
            event = event_log_cls.from_log_row(row)
            events_by_type_by_height.setdefault(event_log_cls, {})[event.block_height] = event

        return events_by_type_by_height

    def _parse_fault_height(self, log: str, executes_by_height: Mapping[Height, LogEventType]) -> int:
        tmp = search(r'\[INFO\] Killed replica R([0-9]+) at view V=([0-9]+)', log)
        fault_height = 0
        if tmp is not None and len(executes_by_height.keys()) > 0:
            fault_height = max(executes_by_height.keys())
        return fault_height

    @property
    def is_primary(self) -> bool:
        return "<EXECUTE, Request" in self.raw_log

    def parse_time_intervals(self) -> Optional[TimeIntervals]:
        """Parse the time intervals from the logs."""
        log_event_types = [
                SendPrePrepareLog,
                RoastInitLog,
                ExecuteActionLog,
                StartSubmitBlock,
                EndSubmitBlock
        ]
        # Cannot proceed with time interval parsing when EndSubmitBlock is missing (old logs)
        if not all(log_event_type in self._events_by_type_by_height for log_event_type in log_event_types):
            return None
        # findall guarantees the order of matchings, so we can iterate with heights
        # zip with match the shortest iterable
        events_split_by_type = [self._events_by_type_by_height[log_event_type].values() for log_event_type in log_event_types]
        result: Dict[Height, Dict[TimeIntervalType, TimeInterval]] = dict()
        for start_fbft_event, start_roast, execute, start_submitblock, end_submitblock in zip(*events_split_by_type):
            assert start_fbft_event.block_height == start_roast.block_height == execute.block_height == start_submitblock.block_height == end_submitblock.block_height
            assert start_fbft_event.log_timestamp <= start_roast.log_timestamp <= execute.log_timestamp <= start_submitblock.log_timestamp <= end_submitblock.log_timestamp
            height = start_fbft_event.block_height
            fbft_time_interval = TimeInterval(
                start_fbft_event.log_timestamp,
                start_roast.log_timestamp,
                TimeIntervalType.FBFT
            )
            roast_time_interval = TimeInterval(
                start_roast.log_timestamp,
                start_submitblock.log_timestamp,
                TimeIntervalType.ROAST
            )
            submitblock_time_interval = TimeInterval(
                start_submitblock.log_timestamp,
                end_submitblock.log_timestamp,
                TimeIntervalType.BITCOIN
            )
            result[height] = {interval.interval_type: interval for interval in [fbft_time_interval, submitblock_time_interval, roast_time_interval]}

        return TimeIntervals(result)


@dataclasses.dataclass(frozen=True)
class NodeLogs:
    """Dataclass that manages access to a list of node logs."""
    logs: Tuple[NodeLog]

    @property
    def no_view_change(self) -> bool:
        """Return True if only one node was the primary for the entire duration of the experiment; False otherwise."""
        primary = [index for index, node_logs in enumerate(self.logs) if node_logs.is_primary]
        assert len(primary) > 0, "no primary detected!"
        return len(primary) == 1

    @property
    def start(self) -> float:
        """Get the start time of the simulation (considering the min timestamp of a receive request event)."""
        return min(request_log.log_timestamp for node_log in self.logs for _, request_log in node_log.requests_by_height.items())

    @property
    def end(self) -> float:
        """Get the end time of the simulation (considering the max timestamp of an execution)."""
        return max(execute_log.log_timestamp for node_log in self.logs for _, execute_log in node_log.executes_by_height.items())

    @property
    def primary_logs(self) -> Optional[NodeLog]:
        if not self.no_view_change:
            # cannot get primary logs, a view change happened
            return None
        primary_index = [index for index, node_logs in enumerate(self.logs) if node_logs.is_primary][0]
        return self.logs[primary_index]

    def get_earliest_log_event_by_height(self, log_event_type: Type[LogEvent], not_before: PosixTimestamp = 0) -> Mapping[Height, LogEvent]:
        """
        Aggregate log events by considering the lowest timestamp among node logs.

        We handle the exception for receive requests event, for which we consider the 'true request time'.

        :param log_event_type: the log event to aggregate
        :param not_before: don't return events before this time (default: allow everything)
        :return: the mapping from height to selected events
        """
        result: Dict[Height, LogEvent] = {}
        banned_heights = set()
        for node_log in self.logs:
            for height, log_event in node_log.events_by_type_by_height.get(log_event_type, {}).items():
                log_timestamp = log_event.true_request_time if isinstance(log_event, ReceiveRequestsLog) else log_event.log_timestamp
                if log_timestamp < not_before:
                    banned_heights.add(height)
                    if height in result:
                        result.pop(height)
                if log_timestamp >= not_before and height not in banned_heights and (
                        height not in result or result[height].log_timestamp > log_event.log_timestamp):
                    result[height] = log_event
        return result

    def get_earliest_requests_by_height(self, not_before: PosixTimestamp = 0) -> Mapping[Height, ReceiveRequestsLog]:
        """
        Aggregate requests log events by considering the lowest timestamp among node logs.

        :param not_before: don't return events before this time (default: allow everything)
        :return: the mapping from height to selected events
        """
        return cast(Mapping[Height, ReceiveRequestsLog], self.get_earliest_log_event_by_height(ReceiveRequestsLog, not_before))

    def get_earliest_executes_by_height(self, not_before: PosixTimestamp = 0) -> Mapping[Height, ExecuteActionLog]:
        """
        Aggregate executes log events by considering the lowest timestamp among node logs.

        :param not_before: don't return events before this time (default: allow everything)
        :return: the mapping from height to selected events
        """
        return cast(Mapping[Height, ExecuteActionLog], self.get_earliest_log_event_by_height(ExecuteActionLog, not_before))

    def get_earliest_preprepare_by_height(self, not_before: PosixTimestamp = 0) -> Mapping[Height, SendPrePrepareLog]:
        """
        Aggregate executes log events by considering the lowest timestamp among node logs.

        :param not_before: don't return events before this time (default: allow everything)
        :return: the mapping from height to selected events
        """
        return cast(Mapping[Height, SendPrePrepareLog], self.get_earliest_log_event_by_height(SendPrePrepareLog, not_before))

    def get_earliest_start_submit_block_by_height(self, not_before: PosixTimestamp = 0) -> Mapping[Height, StartSubmitBlock]:
        """
        Aggregate StartSubmitBlock log events by considering the lowest timestamp among node logs.

        :param not_before: don't return events before this time (default: allow everything)
        :return: the mapping from height to selected events
        """
        return cast(Mapping[Height, StartSubmitBlock], self.get_earliest_log_event_by_height(StartSubmitBlock, not_before))

    def get_fault_height(self) -> int:
        result = []
        for node_log in self.logs:
            result.append(node_log.fault_height)
        assert all(x == result[0] or x == 0 for x in result)
        return result[0]


@dataclasses.dataclass
class TxLog:
    """Dataclass to represent logs on transaction"""
    submitted_timestamp: PosixTimestamp
    mempool_timestamp: PosixTimestamp
    finalized_timestamp: PosixTimestamp
    tx_number: int
    tx_hash: str
    size: int


@dataclasses.dataclass(frozen=True)
class ClientLog:
    """Dataclass to represent client logs."""
    tx_logs: List[TxLog]
    start: PosixTimestamp
    size: int
    rate: float


@dataclasses.dataclass(frozen=True)
class ClientLogs:
    """Dataclass to represent a set of client logs."""
    logs: List[ClientLog]

    @property
    def tx_sizes(self):
        return [client_log.size for client_log in self.logs]


@dataclasses.dataclass(frozen=True)
class RunResult:
    """Dataclass to represent results."""

    bench_parameters: BenchParameters
    node_logs: NodeLogs

    @property
    def time_intervals(self) -> Optional[TimeIntervals]:
        """Get the time intervals from the primary's logs (if a unique primary exists, otherwise None)."""
        primary_logs = self.node_logs.primary_logs
        if primary_logs is None:
            return None
        return self.node_logs.primary_logs.parse_time_intervals()

    @property
    def consensus_block_throughput(self) -> float:
        """
        Compute the block throughput (number of blocks / seconds).

        :return: the block throughput
        """
        earliest_executes_by_height = self.node_logs.get_earliest_executes_by_height(self.node_logs.start + self.bench_parameters.warmup_duration)
        if len(earliest_executes_by_height) == 0:
            return 0
        start, end = self.node_logs.start + self.bench_parameters.warmup_duration, self.node_logs.end
        duration = end - start
        blocks_per_second = len(earliest_executes_by_height) / duration
        return blocks_per_second

    @property
    def latency_at_fault_height(self) -> float:
        if self.bench_parameters.fault_time is None:
            return 0
        earliest_executes_by_height = self.node_logs.get_earliest_executes_by_height()
        earliest_requests_by_height = self.node_logs.get_earliest_requests_by_height()

        fault_height = self.node_logs.get_fault_height()
        latency = 0
        if fault_height is not None:
            latency = earliest_executes_by_height[fault_height+1].log_timestamp - earliest_requests_by_height[fault_height+1].true_request_time
        return latency

    @property
    def consensus_block_latencies_by_height(self) -> Dict[Height, float]:
        """
        Compute the latency of the block consensus.

        We consider the earliest request and the earliest execute, among all nodes.
        This was the same approach used by LibHotStuff.
        """
        earliest_requests_by_height = self.node_logs.get_earliest_requests_by_height(self.node_logs.start + self.bench_parameters.warmup_duration)
        earliest_executes_by_height = self.node_logs.get_earliest_executes_by_height(self.node_logs.start + self.bench_parameters.warmup_duration)
        latencies_by_height = {height: execute_log.log_timestamp - earliest_requests_by_height[height].true_request_time for height, execute_log in earliest_executes_by_height.items() if height in earliest_requests_by_height}
        assert all(latency >= 0.0 for latency in latencies_by_height.values())
        assert len(latencies_by_height), "no blocks!"
        return latencies_by_height

    @property
    def consensus_block_latencies_from_preprepare_by_height(self) -> Dict[Height, float]:
        """
        Compute the latency of the block consensus.

        We consider the earliest send_pre_prepare and the earliest execute, among all nodes.
        This was the same approach used by LibHotStuff.
        """
        earliest_preprepare_by_height = self.node_logs.get_earliest_preprepare_by_height(self.node_logs.start + self.bench_parameters.warmup_duration)
        earliest_executes_by_height = self.node_logs.get_earliest_executes_by_height(self.node_logs.start + self.bench_parameters.warmup_duration)
        latencies_by_height = {height: execute_log.log_timestamp - earliest_preprepare_by_height[height].log_timestamp for height, execute_log in earliest_executes_by_height.items() if height in earliest_preprepare_by_height}
        assert all(latency >= 0.0 for latency in latencies_by_height.values())
        assert len(latencies_by_height), "no blocks!"
        return latencies_by_height

    @property
    def blocksize_by_height(self) -> Dict[Height, int]:
        """
        Retrieve the blocksize at a specific height.
        """
        earliest_blocksize_by_height = self.node_logs.get_earliest_start_submit_block_by_height(self.node_logs.start + self.bench_parameters.warmup_duration)
        blocksizes_by_height = {height: start_submit_block_log.block_size for height, start_submit_block_log in earliest_blocksize_by_height.items()}
        assert all(blocksize >= 0 for blocksize in blocksizes_by_height.values())
        assert len(blocksizes_by_height), "no blocks!"
        return blocksizes_by_height

    @property
    def consensus_block_latencies(self) -> List[float]:
        """Compute the latency of the block consensus."""
        return list(self.consensus_block_latencies_by_height.values())

    @property
    def consensus_block_latencies_from_preprepare(self) -> List[float]:
        """Compute the latency of the block consensus (but computed from preprepare)."""
        return list(self.consensus_block_latencies_from_preprepare_by_height.values())

    @property
    def true_duration(self) -> float:
        """Get true duration from logs."""
        return self.node_logs.end - self.node_logs.start

    @property
    def mean_latency(self) -> float:
        """Get the mean of the latencies."""
        return mean(self.consensus_block_latencies)

    @property
    def std_latency(self) -> float:
        """Get the std of the latencies."""
        return stdev(self.consensus_block_latencies)

    def csv_summary(self, filename) -> None:
        """Write results to csv file."""
        assert isinstance(filename, str)
        # if a unique primary does not exist, time_intervals is None
        time_intervals: Optional[TimeIntervals] = self.time_intervals
        time_spent_by_protocol = self.time_intervals.get_cumulative_time_by_interval_type(self.node_logs.start + self.bench_parameters.warmup_duration) if time_intervals else defaultdict(lambda: float("NaN"))
        file_exists = os.path.isfile(filename)
        data = [self.bench_parameters.nodes,
            self.bench_parameters.faults,
            self.bench_parameters.target_block_time,
            self.bench_parameters.rate,
            self.bench_parameters.genesis_hours_in_the_past,
            self.bench_parameters.tx_size,
            round(self.true_duration),
            round(self.bench_parameters.warmup_duration),
            round(self.consensus_block_throughput, ndigits=5),
            round(mean(self.consensus_block_latencies) * 1000),
            round(stdev(self.consensus_block_latencies) * 1000),
            round(self.latency_at_fault_height * 1000),
            time_spent_by_protocol[TimeIntervalType.FBFT],
            time_spent_by_protocol[TimeIntervalType.ROAST],
            time_spent_by_protocol[TimeIntervalType.BITCOIN]
        ]
        with open(filename, 'a+') as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(RESULT_HEADERS)
            writer.writerow(data)

    def csv_summary_by_height(self, filename: str) -> None:
        # if a unique primary does not exist, time_intervals is None
        time_intervals: Optional[TimeIntervals] = self.time_intervals
        time_spent_by_height = self.time_intervals.intervals_by_height if time_intervals else defaultdict(lambda: float("NaN"))
        consensus_block_latencies_from_requests = self.consensus_block_latencies_by_height
        consensus_block_latencies_from_preprepare = self.consensus_block_latencies_from_preprepare_by_height
        blocksize_by_height = self.blocksize_by_height
        heights = set(consensus_block_latencies_from_preprepare.keys())\
            .intersection(consensus_block_latencies_from_requests.keys())\
            .intersection(blocksize_by_height.keys())

        headers = [
            "Nodes",
            "Faults",
            "Protocol",
            "Target Block Time",
            "Rate",
            "Genesis hours in the past",
            'Tx size',
            "Height",
            "Blocksize",
            "Latency (from request)",
            "Latency (from preprepare)",
            "Time spent in FBFT",
            "Time spent in ROAST",
            "Time spent in Bitcoin",
        ]

        data = []
        for height in heights:
            time_spent_by_protocol = time_spent_by_height[height] if height in time_spent_by_height else None
            blocksize = blocksize_by_height[height] if height in blocksize_by_height else float('nan')
            request_latency = consensus_block_latencies_from_requests[height]
            preprepare_latency = consensus_block_latencies_from_preprepare[height]
            data.append([
                self.bench_parameters.nodes,
                self.bench_parameters.faults,
                self.bench_parameters.target_block_time,
                self.bench_parameters.rate,
                self.bench_parameters.genesis_hours_in_the_past,
                self.bench_parameters.tx_size,
                height,
                blocksize,
                request_latency,
                preprepare_latency,
                time_spent_by_protocol[TimeIntervalType.FBFT].diff if time_spent_by_protocol is not None else float('nan'),
                time_spent_by_protocol[TimeIntervalType.ROAST].diff if time_spent_by_protocol is not None else float('nan'),
                time_spent_by_protocol[TimeIntervalType.BITCOIN].diff if time_spent_by_protocol is not None else float('nan')
            ])
        file_exists = os.path.isfile(filename)
        with open(filename, 'a+') as f:
            writer = csv.writer(f)
            if not file_exists:
                writer.writerow(headers)
            writer.writerows(data)

    @property
    def summary(self) -> str:
        """Get the summary."""
        time_intervals = self.time_intervals
        time_intervals_summary = time_intervals.get_time_spent_summary(self.node_logs.start + self.bench_parameters.warmup_duration) if time_intervals else "No time intervals summary available (view change happened, old logs not containing EndSubmitBlock event).\n"
        return (
            '\n'
            '-----------------------------------------\n'
            ' SUMMARY:\n'
            '-----------------------------------------\n'
            ' + CONFIG:\n'
            f' Faults: {self.bench_parameters.faults} nodes\n'
            f' Committee size: {self.bench_parameters.nodes} nodes\n'
            f' Target block time: {self.bench_parameters.target_block_time} s\n'
            f' Input rate: {self.bench_parameters.rate} tx/s\n'
            f' Genesis hours in the past: {self.bench_parameters.genesis_hours_in_the_past} h\n'
            f' Transaction size: {self.bench_parameters.tx_size} B\n'
            f' Execution time: {round(self.true_duration):,} s\n'
            f' Warm-up time:   {round(self.bench_parameters.warmup_duration):,} s\n'
            '-----------------------------------------\n'
            ' + RESULTS:\n'
            f' Throughput (blocks/s):              {round(self.consensus_block_throughput, ndigits=5):,} blocks/s\n'
            f' latency from request (mean±std):    {round(mean(self.consensus_block_latencies) * 1000):,}±'
            f'{round(stdev(self.consensus_block_latencies) * 1000)} ms\n'
            f' latency from request (  0th):       {round(np.percentile(self.consensus_block_latencies, 0) * 1000)} ms\n'
            f' latency from request ( 25th):       {round(np.percentile(self.consensus_block_latencies, 25) * 1000)} ms\n'
            f' latency from request ( 50th):       {round(np.percentile(self.consensus_block_latencies, 50) * 1000)} ms\n'
            f' latency from request ( 75th):       {round(np.percentile(self.consensus_block_latencies, 75) * 1000)} ms\n'
            f' latency from request (100th):       {round(np.percentile(self.consensus_block_latencies, 100) * 1000)} ms\n'
            f' latency from preprepare (mean/std): {round(mean(self.consensus_block_latencies_from_preprepare) * 1000):,}±'
            f'{round(stdev(self.consensus_block_latencies_from_preprepare) * 1000)} ms\n'
            f' latency from preprepare (  0th):    {round(np.percentile(self.consensus_block_latencies_from_preprepare, 0) * 1000)} ms\n'
            f' latency from preprepare ( 25th):    {round(np.percentile(self.consensus_block_latencies_from_preprepare, 25) * 1000)} ms\n'
            f' latency from preprepare ( 50th):    {round(np.percentile(self.consensus_block_latencies_from_preprepare, 50) * 1000)} ms\n'
            f' latency from preprepare ( 75th):    {round(np.percentile(self.consensus_block_latencies_from_preprepare, 75) * 1000)} ms\n'
            f' latency from preprepare (100th):    {round(np.percentile(self.consensus_block_latencies_from_preprepare, 100) * 1000)} ms\n'
            f' #Blocks:                            {len(self.consensus_block_latencies)}\n'
            f' Latency at fault height:            {round(self.latency_at_fault_height * 1000):,} ms\n'
            + time_intervals_summary
        )

    def print(self, filename):
        assert isinstance(filename, str)
        with open(filename, 'a+') as f:
            f.write(self.summary)


@dataclasses.dataclass(frozen=True)
class ExperimentResult:
    """Dataclass to represent experiment results."""
    run_results: List[RunResult]

    @classmethod
    def from_directory(cls, experiment_directory: Path) -> "ExperimentResult":
        """Parse an ExperimentResult object from an experiment directory."""
        run_results: List[RunResult] = []
        for run_directory in sorted(experiment_directory.iterdir()):
            log_results = LogParser.process(run_directory, experiment_directory).result()
            run_results.append(log_results)

        # check bench parameters are all the same
        assert all(result.bench_parameters == run_results[0].bench_parameters for result in run_results)
        return ExperimentResult(run_results)

    @property
    def bench_parameters(self) -> BenchParameters:
        """Get the benchmark parameters used for this experiment."""
        # we take the bench parameters of the first run since we have already checked
        #  they are all the same
        return self.run_results[0].bench_parameters

    @property
    def mean_latencies(self) -> List[float]:
        """Get the mean (taken within a run) latency of the runs."""
        return [result.mean_latency for result in self.run_results]

    @property
    def throughputs(self) -> List[float]:
        """Get the list of throughputs of the runs."""
        return [result.consensus_block_throughput for result in self.run_results]

    @property
    def mean_mean_latency(self) -> float:
        """Take the mean (across runs) of the mean (across blocks within the same run) of the latencies."""
        return mean(self.mean_latencies)

    @property
    def mean_std_latency(self) -> float:
        """Take the mean (across runs) of the stdev (across blocks within the same run) of the latencies."""
        return stdev(self.mean_latencies)

    @property
    def mean_mean_throughputs(self) -> float:
        """Take the mean (across runs) of the mean (within the same run) consensus block throughput."""
        return mean(self.throughputs)

    @property
    def mean_std_throughputs(self) -> float:
        """Take the mean (across runs) of the stdev (within the same run) consensus block throughput."""
        return stdev(self.throughputs)


class LogParser:

    node_logs: NodeLogs
    output_directory : Path

    def __init__(self, clients: List[str], nodes: List[str], bench_parameters: BenchParameters, output_directory: Path):
        inputs = [clients, nodes]
        assert all(isinstance(x, list) for x in inputs)
        assert all(isinstance(x, str) for y in inputs for x in y)
        assert all(x for x in inputs)
        assert len(clients) == bench_parameters.clients

        self.bench_parameters = bench_parameters
        self.output_directory = output_directory

        # Parse the clients logs.
        try:
            with Pool() as p:
                client_logs = p.map(self._parse_clients, clients)
        except (ValueError, IndexError) as e:
            raise ParseError(f'Failed to parse client logs: {e}')
        self.client_logs = ClientLogs(client_logs)
        self.rate = sum(log.rate for log in self.client_logs.logs)
        # Parse the nodes logs.
        try:
            with Pool() as p:
                node_logs = p.map(self._parse_nodes, nodes)
        except (ValueError, IndexError) as e:
            raise ParseError(f'Failed to parse node logs: {e}')
        self.node_logs = NodeLogs(tuple(node_logs))

        # TODO: add client logs parsing
        # # Check whether clients missed their target rate.
        # if self.misses != 0:
        #     Print.warn(
        #         f'Clients missed their target rate {self.misses:,} time(s)'
        #     )

        # TODO: detect timed out nodes
        # Check whether the nodes timed out.
        # Note that nodes are expected to time out once at the beginning.
        # if self.timeouts > 2:
        #     Print.warn(f'Nodes timed out {self.timeouts:,} time(s)')

    def _parse_clients(self, log):
        if search(r'Error', log) is not None:
            raise ParseError('Client(s) panicked')

        size = int(search(r'Transactions size: (\d+)', log).group(1))
        rate = float(search(r'Transactions rate: (\d+)', log).group(1))

        tmp = search(r'\[(.*?)].*Start sending transactions', log).group(1)
        start = self.to_posix(tmp)

        tmp = findall(r'\[(.*?)] .*Sending transaction number (\d+) with hash (.*) of size (.*)', log)
        tx_logs_by_number: Dict[int, TxLog] = {int(tx_number) : TxLog(self.to_posix(raw_timestamp), -1, -1, int(tx_number), tx_hash, int(tx_size)) for raw_timestamp, tx_number, tx_hash, tx_size, in tmp}
        tx_hash_to_number: Dict[str, int] = {tx_log.tx_hash: tx_log.tx_number for tx_log in tx_logs_by_number.values()}

        tmp = findall(r'\[(.*?)] .*seqnumber: .*, tx hash=(.*)', log)
        # process lines in timestamp order: first time means entered into mempool;
        # second time means finalized into a block
        for timestamp, tx_hash in sorted(tmp, key=lambda x: x[0]):
            if tx_hash not in tx_hash_to_number:
                # not a client tx, ignore it
                continue
            tx_log_to_be_updated = tx_logs_by_number[tx_hash_to_number[tx_hash]]
            assert tx_log_to_be_updated.tx_hash == tx_hash
            if tx_log_to_be_updated.mempool_timestamp == UNDEFINED_TIMESTAMP:
                # first time we see it in the log; it means it is a mempool timestamp
                tx_log_to_be_updated.mempool_timestamp = self.to_posix(timestamp)
            else:
                assert tx_log_to_be_updated.finalized_timestamp == UNDEFINED_TIMESTAMP
                tx_log_to_be_updated.finalized_timestamp = self.to_posix(timestamp)

        tx_logs = sorted(tx_logs_by_number.values(), key=attrgetter("tx_number"))
        return ClientLog(tx_logs, start, size, rate)

    def _parse_nodes(self, log) -> NodeLog:
        if search(r'panic', log) is not None:
            raise ParseError('Node(s) panicked')
        return NodeLog(log, {})

    @staticmethod
    def to_posix(string):
        # TODO:
        # This function assumes that log timestamps are in GPT. This is not the case, e.g. when running locally.
        # Nevertheless, being results calculated using differences between timestamps, they should not be affected.
        epoch = datetime.datetime(1970, 1, 1)
        try:
            timestamp = (datetime.datetime.strptime(string, "%Y-%b-%d %H:%M:%S.%f") - epoch).total_seconds()
        except ValueError:
            timestamp = (datetime.datetime.strptime(string, "%Y-%b-%d %H:%M:%S") - epoch).total_seconds()

        return timestamp

    def result(self) -> RunResult:
        result = RunResult(self.bench_parameters, self.node_logs)
        result.csv_summary(str(self.output_directory) + '/' + PathMaker.csv_result_file())
        result.csv_summary_by_height(str(self.output_directory) + '/' + PathMaker.csv_by_height_result_file())
        return result

    def print(self, filename):
        assert isinstance(filename, str)
        with open(filename, 'a') as f:
            f.write(self.result().summary)

    @classmethod
    def process(cls, run_directory: PathLike, output_directory: PathLike):
        run_directory = Path(run_directory)

        clients = []
        for filename in sorted(glob(join(run_directory, "client*/client_logs.txt"))):
            with open(filename, 'r') as f:
                clients += [f.read()]

        nodes = []
        # note, 'sorted' works because of alphanumerical ordering
        for filename in sorted(glob(join(run_directory, "node*/miner_logs.txt"))):
            with open(filename, 'r') as f:
                nodes += [f.read()]

        bench_parameters = BenchParameters.from_file(run_directory / DEFAULT_BENCH_PARAMETERS_FILENAME)
        return cls(clients, nodes, bench_parameters, output_directory)
