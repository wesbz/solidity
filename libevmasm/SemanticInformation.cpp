/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * @file SemanticInformation.cpp
 * @author Christian <c@ethdev.com>
 * @date 2015
 * Helper to provide semantic information about assembly items.
 */

#include <libevmasm/SemanticInformation.h>
#include <libevmasm/AssemblyItem.h>

using namespace std;
using namespace solidity;
using namespace solidity::evmasm;

vector<SemanticInformation::Operation> SemanticInformation::readWriteOperations(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::SSTORE:
	case Instruction::SLOAD:
	case Instruction::MSTORE:
	case Instruction::MSTORE8:
	case Instruction::MLOAD:
	case Instruction::REVERT:
	case Instruction::RETURN:
	case Instruction::EXTCODECOPY:
	case Instruction::CODECOPY:
	case Instruction::CALLDATACOPY:
	case Instruction::RETURNDATACOPY:
	case Instruction::KECCAK256:
	case Instruction::LOG0:
	case Instruction::LOG1:
	case Instruction::LOG2:
	case Instruction::LOG3:
	case Instruction::LOG4:
	{
		Operation op;
		if (memory(_instruction) == SideEffects::Write || storage(_instruction) == SideEffects::Write)
			op.effect = Effect::Write;
		else
			op.effect = Effect::Read;

		op.location =
			(_instruction == Instruction::SSTORE || _instruction == Instruction::SLOAD) ?
			Location::Storage :
			Location::Memory;

		if (_instruction == Instruction::EXTCODECOPY)
			op.startParameter = 1;
		else
			op.startParameter = 0;

		if (_instruction == Instruction::MSTORE || _instruction == Instruction::MLOAD)
			op.lengthConstant = 32;
		else if (instruction == Instruction::MSTORE8)
			op.lengthConstant = 1;
		else if (
			_instruction == Instruction::REVERT ||
			_instruction == Instruction::RETURN ||
			_instruction == Instruction::KECCAK256 ||
			_instruction == Instruction::LOG0 ||
			_instruction == Instruction::LOG1 ||
			_instruction == Instruction::LOG2 ||
			_instruction == Instruction::LOG3 ||
			_instruction == Instruction::LOG4
		)
			op.lengthParameter = 1;
		else if (_instruction == Instruction::EXTCODECOPY)
			op.lengthParameter = 3;
		else if (
			_instruction == Instruction::CALLDATACOPY ||
			_instruction == Instruction::CODECOPY ||
			_instruction == Instruction::RETURNDATACOPY
		)
			op.lengthParameter = 2;
		else if (_instruction == Instruction::SSTORE || _instruction == Instruction::SLOAD)
			// Storage operations, length is unused / non-sensical
			op.lengthConstant = 1;
		else
			assertThrow(false, AssemblyException, "");

		return {op};
	}
	case Instruction::STATICCALL:
	case Instruction::CALL:
	case Instruction::CALLCODE:
	case Instruction::DELEGATECALL:
	{
		size_t arguments = _functionCall.arguments.size();
		return vector<Operation>{
			Operation{
				Location::Memory,
				Effect::Read,
				paramCount - 4,
				paramCount - 3,
				{}
			},
			// TOOD is that fine here?
			// TODO in the unused store eliminator we did not have the write operation.
			// TODO do we want to add another read?
			Operation{Location::Storage, Effect::Read, {}, {}},
			Operation{Location::Storage, Effect::Write, {}, {}},
			Operation{
				Location::Memory,
				Effect::Write,
				paramCount - 2,
				paramCount - 1,
				{}
			}
		};
	}
	case Instruction::CREATE:
	case Instruction::CREATE2:
		return vector<Operation>{
			Operation{
				Location::Memory,
				Effect::Read,
				1,
				2,
				{}
			},
			// TODO
			// Unknown read includes unknown write.
			Operation{Location::Storage, Effect::Read, {}, {}, {}},
			Operation{Location::Storage, Effect::Write, {}, {}, {}},
		};
	case Instruction::MSIZE:
		// This is just to satisfy the assert below.
		return vector<Operation>{};
	default:
		break;
	}

	assertThrow(storage(_instruction) == None && memory(_instruction) == None, AssemblyException, "");
}

bool SemanticInformation::breaksCSEAnalysisBlock(AssemblyItem const& _item, bool _msizeImportant)
{
	switch (_item.type())
	{
	default:
	case UndefinedItem:
	case Tag:
	case PushDeployTimeAddress:
	case AssignImmutable:
	case VerbatimBytecode:
		return true;
	case Push:
	case PushTag:
	case PushSub:
	case PushSubSize:
	case PushProgramSize:
	case PushData:
	case PushLibraryAddress:
	case PushImmutable:
		return false;
	case Operation:
	{
		if (isSwapInstruction(_item) || isDupInstruction(_item))
			return false;
		if (_item.instruction() == Instruction::GAS || _item.instruction() == Instruction::PC)
			return true; // GAS and PC assume a specific order of opcodes
		if (_item.instruction() == Instruction::MSIZE)
			return true; // msize is modified already by memory access, avoid that for now
		InstructionInfo info = instructionInfo(_item.instruction());
		if (_item.instruction() == Instruction::SSTORE)
			return false;
		if (_item.instruction() == Instruction::MSTORE)
			return false;
		if (!_msizeImportant && (
			_item.instruction() == Instruction::MLOAD ||
			_item.instruction() == Instruction::KECCAK256
		))
			return false;
		//@todo: We do not handle the following memory instructions for now:
		// calldatacopy, codecopy, extcodecopy, mstore8,
		// msize (note that msize also depends on memory read access)

		// the second requirement will be lifted once it is implemented
		return info.sideEffects || info.args > 2;
	}
	}
}

bool SemanticInformation::isCommutativeOperation(AssemblyItem const& _item)
{
	if (_item.type() != Operation)
		return false;
	switch (_item.instruction())
	{
	case Instruction::ADD:
	case Instruction::MUL:
	case Instruction::EQ:
	case Instruction::AND:
	case Instruction::OR:
	case Instruction::XOR:
		return true;
	default:
		return false;
	}
}

bool SemanticInformation::isDupInstruction(AssemblyItem const& _item)
{
	if (_item.type() != Operation)
		return false;
	return evmasm::isDupInstruction(_item.instruction());
}

bool SemanticInformation::isSwapInstruction(AssemblyItem const& _item)
{
	if (_item.type() != Operation)
		return false;
	return evmasm::isSwapInstruction(_item.instruction());
}

bool SemanticInformation::isJumpInstruction(AssemblyItem const& _item)
{
	return _item == Instruction::JUMP || _item == Instruction::JUMPI;
}

bool SemanticInformation::altersControlFlow(AssemblyItem const& _item)
{
	if (_item.type() != Operation)
		return false;
	switch (_item.instruction())
	{
	// note that CALL, CALLCODE and CREATE do not really alter the control flow, because we
	// continue on the next instruction
	case Instruction::JUMP:
	case Instruction::JUMPI:
	case Instruction::RETURN:
	case Instruction::SELFDESTRUCT:
	case Instruction::STOP:
	case Instruction::INVALID:
	case Instruction::REVERT:
		return true;
	default:
		return false;
	}
}

bool SemanticInformation::terminatesControlFlow(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::RETURN:
	case Instruction::SELFDESTRUCT:
	case Instruction::STOP:
	case Instruction::INVALID:
	case Instruction::REVERT:
		return true;
	default:
		return false;
	}
}

bool SemanticInformation::reverts(Instruction _instruction)
{
	switch (_instruction)
	{
		case Instruction::INVALID:
		case Instruction::REVERT:
			return true;
		default:
			return false;
	}
}

bool SemanticInformation::isDeterministic(AssemblyItem const& _item)
{
	assertThrow(_item.type() != VerbatimBytecode, AssemblyException, "");

	if (_item.type() != Operation)
		return true;

	switch (_item.instruction())
	{
	case Instruction::CALL:
	case Instruction::CALLCODE:
	case Instruction::DELEGATECALL:
	case Instruction::STATICCALL:
	case Instruction::CREATE:
	case Instruction::CREATE2:
	case Instruction::GAS:
	case Instruction::PC:
	case Instruction::MSIZE: // depends on previous writes and reads, not only on content
	case Instruction::BALANCE: // depends on previous calls
	case Instruction::SELFBALANCE: // depends on previous calls
	case Instruction::EXTCODESIZE:
	case Instruction::EXTCODEHASH:
	case Instruction::RETURNDATACOPY: // depends on previous calls
	case Instruction::RETURNDATASIZE:
		return false;
	default:
		return true;
	}
}

bool SemanticInformation::movable(Instruction _instruction)
{
	// These are not really functional.
	if (isDupInstruction(_instruction) || isSwapInstruction(_instruction))
		return false;
	InstructionInfo info = instructionInfo(_instruction);
	if (info.sideEffects)
		return false;
	switch (_instruction)
	{
	case Instruction::KECCAK256:
	case Instruction::BALANCE:
	case Instruction::SELFBALANCE:
	case Instruction::EXTCODESIZE:
	case Instruction::EXTCODEHASH:
	case Instruction::RETURNDATASIZE:
	case Instruction::SLOAD:
	case Instruction::PC:
	case Instruction::MSIZE:
	case Instruction::GAS:
		return false;
	default:
		return true;
	}
	return true;
}

bool SemanticInformation::canBeRemoved(Instruction _instruction)
{
	// These are not really functional.
	assertThrow(!isDupInstruction(_instruction) && !isSwapInstruction(_instruction), AssemblyException, "");

	return !instructionInfo(_instruction).sideEffects;
}

bool SemanticInformation::canBeRemovedIfNoMSize(Instruction _instruction)
{
	if (_instruction == Instruction::KECCAK256 || _instruction == Instruction::MLOAD)
		return true;
	else
		return canBeRemoved(_instruction);
}

SemanticInformation::Effect SemanticInformation::memory(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::CALLDATACOPY:
	case Instruction::CODECOPY:
	case Instruction::EXTCODECOPY:
	case Instruction::RETURNDATACOPY:
	case Instruction::MSTORE:
	case Instruction::MSTORE8:
	case Instruction::CALL:
	case Instruction::CALLCODE:
	case Instruction::DELEGATECALL:
	case Instruction::STATICCALL:
		return SemanticInformation::Write;

	case Instruction::CREATE:
	case Instruction::CREATE2:
	case Instruction::KECCAK256:
	case Instruction::MLOAD:
	case Instruction::MSIZE:
	case Instruction::RETURN:
	case Instruction::REVERT:
	case Instruction::LOG0:
	case Instruction::LOG1:
	case Instruction::LOG2:
	case Instruction::LOG3:
	case Instruction::LOG4:
		return SemanticInformation::Read;

	default:
		return SemanticInformation::None;
	}
}

bool SemanticInformation::movableApartFromEffects(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::EXTCODEHASH:
	case Instruction::EXTCODESIZE:
	case Instruction::RETURNDATASIZE:
	case Instruction::BALANCE:
	case Instruction::SELFBALANCE:
	case Instruction::SLOAD:
	case Instruction::KECCAK256:
	case Instruction::MLOAD:
		return true;

	default:
		return movable(_instruction);
	}
}

SemanticInformation::Effect SemanticInformation::storage(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::CALL:
	case Instruction::CALLCODE:
	case Instruction::DELEGATECALL:
	case Instruction::CREATE:
	case Instruction::CREATE2:
	case Instruction::SSTORE:
		return SemanticInformation::Write;

	case Instruction::SLOAD:
	case Instruction::STATICCALL:
		return SemanticInformation::Read;

	default:
		return SemanticInformation::None;
	}
}

SemanticInformation::Effect SemanticInformation::otherState(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::CALL:
	case Instruction::CALLCODE:
	case Instruction::DELEGATECALL:
	case Instruction::CREATE:
	case Instruction::CREATE2:
	case Instruction::SELFDESTRUCT:
	case Instruction::STATICCALL: // because it can affect returndatasize
		// Strictly speaking, log0, .., log4 writes to the state, but the EVM cannot read it, so they
		// are just marked as having 'other side effects.'
		return SemanticInformation::Write;

	case Instruction::EXTCODESIZE:
	case Instruction::EXTCODEHASH:
	case Instruction::RETURNDATASIZE:
	case Instruction::BALANCE:
	case Instruction::SELFBALANCE:
	case Instruction::RETURNDATACOPY:
	case Instruction::EXTCODECOPY:
		// PC and GAS are specifically excluded here. Instructions such as CALLER, CALLVALUE,
		// ADDRESS are excluded because they cannot change during execution.
		return SemanticInformation::Read;

	default:
		return SemanticInformation::None;
	}
}

bool SemanticInformation::invalidInPureFunctions(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::ADDRESS:
	case Instruction::SELFBALANCE:
	case Instruction::BALANCE:
	case Instruction::ORIGIN:
	case Instruction::CALLER:
	case Instruction::CALLVALUE:
	case Instruction::CHAINID:
	case Instruction::BASEFEE:
	case Instruction::GAS:
	case Instruction::GASPRICE:
	case Instruction::EXTCODESIZE:
	case Instruction::EXTCODECOPY:
	case Instruction::EXTCODEHASH:
	case Instruction::BLOCKHASH:
	case Instruction::COINBASE:
	case Instruction::TIMESTAMP:
	case Instruction::NUMBER:
	case Instruction::DIFFICULTY:
	case Instruction::GASLIMIT:
	case Instruction::STATICCALL:
	case Instruction::SLOAD:
		return true;
	default:
		break;
	}
	return invalidInViewFunctions(_instruction);
}

bool SemanticInformation::invalidInViewFunctions(Instruction _instruction)
{
	switch (_instruction)
	{
	case Instruction::SSTORE:
	case Instruction::JUMP:
	case Instruction::JUMPI:
	case Instruction::LOG0:
	case Instruction::LOG1:
	case Instruction::LOG2:
	case Instruction::LOG3:
	case Instruction::LOG4:
	case Instruction::CREATE:
	case Instruction::CALL:
	case Instruction::CALLCODE:
	case Instruction::DELEGATECALL:
	case Instruction::CREATE2:
	case Instruction::SELFDESTRUCT:
		return true;
	default:
		break;
	}
	return false;
}
