# Copyright (c) 2023 Bank of Italy
# Distributed under the GNU AGPLv3 software license, see the accompanying COPYING file.

import dataclasses
import importlib.machinery
import importlib.util
import json
import math
import shutil
import sys
import types
from abc import ABC
from dataclasses import MISSING
from enum import Enum
from functools import partial
from json import dump
from pathlib import Path
from typing import Union, Generic, TypeVar, Dict, Optional, Type, Sequence, Callable, Any

import click


PathLike = Union[Path, str]
T = TypeVar("T")

def byzantine_quorum(N):
    F = math.floor( (N-1) / 3 )
    return 2*F+1

def byzantine_replies_quorum(N):
    F = math.floor( (N-1) / 3 )
    return F+1

def import_module_by_abspath(
    path_to_source: Path, module_name: str
) -> types.ModuleType:
    """Dynamically imports a python source file and returns it as a module.

    path_to_source must be an absolute path to an existing python file. It is
    not necessary that its name ends with ".py". The path is canonicalized,
    resolving any ".." and symlinks. It is then loaded as a module, added to
    sys.modules[module_name], and returned by the function.

    Inspired by:
    - https://csatlas.com/python-import-file-module/
    - https://docs.python.org/3/library/importlib.html#importing-a-source-file-directly

    EXAMPLE:
        # Fancy version of "import my_module", where my_module is taken from
        # "/etc/some_python_file":

        my_module = import_module_by_abspath('/etc/some_python_file')
        my_module.some_function()
    """
    p = Path(path_to_source)

    if p.is_absolute() == False:
        raise ValueError(
            f'path_to_source must be an absolute path. "{path_to_source} is not"'
        )

    canonical_path = p.resolve(strict=True)

    if module_name in sys.modules:
        # do not load the same file again
        #
        # We cannot print anything on stderr, otherwise the bitcoin test
        # framework thinks tests have failed.
        # print(f'Module "{MODULE_NAME}" is already loaded. Not reloading.', file=sys.stderr)
        return sys.modules[module_name]

    spec = importlib.util.spec_from_loader(
        module_name,
        importlib.machinery.SourceFileLoader(
            module_name,
            str(canonical_path),
        ),
    )
    module = importlib.util.module_from_spec(spec)
    sys.modules[module_name] = module
    spec.loader.exec_module(module)
    return module


class BenchError(Exception):
    def __init__(self, message, error):
        assert isinstance(error, Exception)
        self.message = message
        self.cause = error
        super().__init__(message)


class ConfigError(Exception):
    """A user-defined execption for configuration-related errors."""
    pass


class Color:
    HEADER = '\033[95m'
    OK_BLUE = '\033[94m'
    OK_GREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    END = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'


class Print:
    @staticmethod
    def heading(message):
        assert isinstance(message, str)
        print(f'{Color.OK_GREEN}{message}{Color.END}')

    @staticmethod
    def info(message):
        assert isinstance(message, str)
        print(message)

    @staticmethod
    def cmd(message):
        assert (isinstance(message, str) or isinstance(message, list))
        print(f'{Color.OK_BLUE}{message}{Color.END}')

    @staticmethod
    def warn(message):
        assert isinstance(message, str)
        print(f'{Color.BOLD}{Color.WARNING}WARN{Color.END}: {message}')

    @staticmethod
    def error(e):
        assert isinstance(e, BenchError)
        print(f'\n{Color.BOLD}{Color.FAIL}ERROR{Color.END}: {e}\n')
        causes, current_cause = [], e.cause
        while isinstance(current_cause, BenchError):
            causes += [f'  {len(causes)}: {e.cause}\n']
            current_cause = current_cause.cause
        causes += [f'  {len(causes)}: {type(current_cause)}\n']
        causes += [f'  {len(causes)}: {current_cause}\n']
        print(f'Caused by: \n{"".join(causes)}\n')


def progress_bar(iterable, prefix='', suffix='', decimals=1, length=30, fill='█', print_end='\r'):
    total = len(iterable)

    def printProgressBar(iteration):
        formatter = '{0:.'+str(decimals)+'f}'
        percent = formatter.format(100 * (iteration / float(total)))
        filledLength = int(length * iteration // total)
        bar = fill * filledLength + '-' * (length - filledLength)
        print(f'\r{prefix} |{bar}| {percent}% {suffix}', end=print_end)

    printProgressBar(0)
    for i, item in enumerate(iterable):
        yield item
        printProgressBar(i + 1)
    print()


def ask_before_removing_directory(directory_to_remove: Path) -> bool:
    """Ask before removing the directory."""
    return click.confirm(
        f"Are you sure you want to remove directory {directory_to_remove}?",
        default=False,
    )


def remove_dir_or_fail(output_dir: Path, force: bool) -> None:
    """Remove the directory or fail."""
    if force:
        shutil.rmtree(output_dir, ignore_errors=True)
        return
    if output_dir.exists():
        if ask_before_removing_directory(output_dir):
            shutil.rmtree(output_dir)
        else:
            click.echo("Directory not removed, cannot continue.")
            sys.exit(1)

class NodeType(Enum):
    VALIDATOR = "validator"
    CLIENT = "client"

    def get_dirname(self) -> str:
        """Get the dirname for this node type."""
        return "client" if self == self.CLIENT else "node"


def add_zeros_prefix(n: int, max_n: int) -> str:
    """Format a number with a number of leading zeros."""
    max_nb_digits = len(str(max(max_n - 1, 0)))
    format_string = f"{{:0{max_nb_digits}}}"
    formatted_number = format_string.format(n)
    return formatted_number


def get_node_dirname(node_id: int, node_type: NodeType, total_nb_nodes: int) -> str:
    """Get a node dirname with sufficient leading zeros to keep alphanumeric ordering."""
    final_node_number = add_zeros_prefix(node_id, total_nb_nodes)
    final_node_name = node_type.get_dirname() + "-" + final_node_number
    return final_node_name

import logging

def get_logger(fd):
    # create logger
    logger = logging.getLogger(__name__)
    logger.setLevel(logging.DEBUG)

    # create console handler and set level to debug
    ch = logging.StreamHandler(fd if fd else sys.stdout)
    ch.setLevel(logging.DEBUG)

    # create formatter
    formatter = logging.Formatter('[%(asctime)s.%(msecs)d] [%(levelname)s] %(message)s',
                                  datefmt="%Y-%b-%d %H:%M:%S")

    # add formatter to ch
    ch.setFormatter(formatter)

    # add ch to logger
    logger.addHandler(ch)
    return logger


def check_int_in_range(value: int, range_: range) -> None:
    """Check that an integer is in a range."""
    if not (value in range_):
        raise ValueError(f"expected 'value' between {range_.start} and {range_.stop - 1} included, got {value=}")


FieldValidator = Callable[[Any, T], None]


class ConvertToSet(Generic[T]):
    """A descriptor-typed field class that converts a container into a Set"""

    def __init__(self, validators: Sequence[FieldValidator] = (), frozen: bool = False) -> None:
        """Initialize a descriptor-typed field."""
        self.validators = validators
        self.frozen = frozen

    def __set_name__(self, owner, name):
        self._attr_name = "_" + name

    def __get__(self, instance, owner):
        if instance is None:
            return MISSING
        return self._value

    def __set__(self, instance, value: T):
        if not isinstance(value, (list, tuple, set, frozenset)):
            raise ValueError(f"value of type {type(value)} for attribute {self._attr_name} cannot be converted into a set")
        if len(value) != len(set(value)):
            raise ValueError(f"container given in input  for attribute {self._attr_name} has duplicates: {value}")
        for v in value:
            for validator in self.validators:
                validator(instance, v)
        self._value = frozenset(value) if self.frozen else set(value)


class CustomField(Generic[T]):

    def __init__(self, type_: Type[T], default=MISSING, validators: Sequence[FieldValidator] = (), is_nullable: bool = False) -> None:
        """Initialize a custom field for dataclasses."""
        self._default = default
        self._type = type_
        self._validators = validators
        self._is_nullable = is_nullable
        self._value = None

        if default is None and not is_nullable:
            raise ValueError(f"default is None but attribute is not nullable")

        if default is not None and default != MISSING and not isinstance(default, self._type):
            raise ValueError(f"default value {default} not of type {self._type}")

    def __set_name__(self, owner, name) -> None:
        self._original_name = name
        self._name = "_" + name

    def __get__(self, obj, owner) -> T:
        if obj is None:
            return self._default

        result = getattr(obj, self._name, self._default)

        if result is None and not self._is_nullable:
            raise ValueError(f"attribute {self._original_name} is None but specified as non-nullable")

        if result == MISSING:
            raise ValueError(f"specify a default value for attribute {self._original_name} or class {owner}")

        return result

    def __set__(self, instance, value: T) -> None:
        self._check_isinstance(value)
        self._validate(instance, value)
        object.__setattr__(instance, self._name, value)

    def _validate(self, instance: Any, value: T) -> None:
        """Validate attribute."""
        for validator in self._validators:
            validator(instance, value)

    def _check_isinstance(self, value: Any) -> None:
        if value is not None and not isinstance(value, self._type):
            raise ValueError(f"value {value} with name {self._original_name} is not of type {self._type}")

class Bool(CustomField[bool]):

    def __init__(self, default=MISSING, validators: Sequence[Callable[[Any, T], None]] = ()) -> None:
        """Initialize a boolean field."""
        super().__init__(type_=bool, default=default, validators=validators)


class Int(CustomField[int]):

    def __init__(self,
                 default=MISSING,
                 min_value: Optional[int] = None,
                 max_value: Optional[int] = None,
                 validators: Sequence[FieldValidator] = (),
                 is_nullable: bool = False
                 ) -> None:
        """Initialize an integer field."""
        self._min_value = min_value
        self._max_value = max_value
        _validators = []
        if min_value is not None:
            _validators.append(partial(self._check_min, min_value=self._min_value))
        if max_value is not None:
            _validators.append(partial(self._check_max, max_value=self._max_value))
        validators = tuple(_validators) + tuple(validators)
        super().__init__(type_=int, default=default, validators=validators, is_nullable=is_nullable)

    def _check_min(self, _instance: Any, value: Optional[int], min_value: int):
        if value is not None and not value >= min_value:
            raise ValueError(f"expected value >= min_value for attribute {self._original_name}, got: {value=}, {min_value=}")

    def _check_max(self, _instance: Any, value: Optional[int], max_value: int):
        if value is not None and not value <= max_value:
            raise ValueError(f"expected value >= min_value for attribute {self._original_name}, got: {value=}, {max_value=}")


class PositiveInt(Int):
    """An integer >= 1."""

    def __init__(self, default=MISSING) -> None:
        super().__init__(default=default, min_value=1, validators=())


class NonnegativeInt(Int):
    """An integer >= 0."""

    def __init__(self, default=MISSING) -> None:
        super().__init__(default=default, min_value=0, validators=())


class Float(CustomField[float]):
    """A float field.

    Can also be initialized with int literals.
    """

    def __init__(self, default=MISSING, validators: Sequence[FieldValidator] = (), is_nullable: bool = False) -> None:
        super().__init__(type_=float, default=default, validators=validators, is_nullable=is_nullable)

    def __set__(self, instance, value: T) -> None:
        # convert to float; we check if None because the field can be nullable
        if value is not None and isinstance(value, int):
            value = float(value)
        super().__set__(instance, value)


class SetEncoder(json.JSONEncoder):
    """
    A JSON encoder that supports fields as sets.

    When a set value is encountered, it is converted into a sorted list.
    """
    def default(self, obj):
        if isinstance(obj, set):
            return sorted(obj)
        return json.JSONEncoder.default(self, obj)


def check_greater_than(_instance: Any, value: int, min_value: int) -> None:
    if not value >= min_value:
        raise ValueError(f"value {value} must be greater than {min_value}")


@dataclasses.dataclass(frozen=True)
class BaseParameters(ABC):

    @classmethod
    def from_file(cls, filename: PathLike) -> "BaseParameters":
        """Load benchmark parameters from a JSON file."""
        filename = Path(filename)
        with filename.open('r') as f:
            obj = json.load(f)
        return cls.from_dict(obj)

    def print(self, filename: PathLike):
        """Dump benchmark parameters to a JSON file."""
        filename = Path(filename)
        with filename.open('w') as f:
            dump(self.dict, f, indent=4, sort_keys=True, cls=SetEncoder)

    @classmethod
    def from_dict(cls, obj: Dict) -> "BaseParameters":
        """Load benchmark parameters from a Dict.

        The function takes a python dict and converts it to a dataclass. The
        source dict must have keys for all the required fields of the
        destination object.

        If for a field of the dataclass is already defined a default value, the
        dict does not need to contain that key.

        Every spurious key in the source dict is ignored.
        """
        filtered = dict()
        for field in dataclasses.fields(cls):
            try:
                # for each field of the dataclass that does not have a default
                # value, take the corresponding field from obj, ignoring the
                # others.
                filtered[field.name] = obj[field.name]
            except KeyError as e:
                # complain only if the field does not have a default value
                if field.default is MISSING and field.default_factory is MISSING:
                    raise ConfigError(f"malformed bench parameters: missing key {e}") from None
            except ValueError:
                raise ConfigError("Invalid parameters type") from None
        return cls(**filtered)

    @property
    def dict(self) -> Dict:
        """Serialize the object in JSON."""
        return dataclasses.asdict(self)
