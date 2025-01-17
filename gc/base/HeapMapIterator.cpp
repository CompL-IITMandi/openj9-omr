/*******************************************************************************
 * Copyright IBM Corp. and others 1991
 *
 * This program and the accompanying materials are made available under
 * the terms of the Eclipse Public License 2.0 which accompanies this
 * distribution and is available at https://www.eclipse.org/legal/epl-2.0/
 * or the Apache License, Version 2.0 which accompanies this distribution and
 * is available at https://www.apache.org/licenses/LICENSE-2.0.
 *
 * This Source Code may also be made available under the following
 * Secondary Licenses when the conditions for such availability set
 * forth in the Eclipse Public License, v. 2.0 are satisfied: GNU
 * General Public License, version 2 with the GNU Classpath
 * Exception [1] and GNU General Public License, version 2 with the
 * OpenJDK Assembly Exception [2].
 *
 * [1] https://www.gnu.org/software/classpath/license.html
 * [2] https://openjdk.org/legal/assembly-exception.html
 *
 * SPDX-License-Identifier: EPL-2.0 OR Apache-2.0 OR GPL-2.0 WITH Classpath-exception-2.0 OR LicenseRef-GPL-2.0 WITH Assembly-exception
 *******************************************************************************/
#include "HeapMapIterator.hpp"

#include "Bits.hpp"
#include "GCExtensionsBase.hpp"
#include "HeapMap.hpp"
#include "Math.hpp"
#include "ObjectModel.hpp"


bool
MM_HeapMapIterator::setHeapMap(MM_HeapMap *heapMap)
{
	uintptr_t heapOffsetInBytes = (uintptr_t)_heapSlotCurrent - (uintptr_t)heapMap->getHeapBase();

	_bitIndexHead = heapMap->getBitIndex((omrobjectptr_t)_heapSlotCurrent);

	_heapMapSlotCurrent = (uintptr_t *) ( ((uint8_t *)heapMap->getHeapMapBits())
		+ (MM_Math::roundToFloor(J9MODRON_HMI_HEAPMAP_ALIGNMENT, heapOffsetInBytes) / J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT) );

	/* Cache the first heap map value ONLY if we are starting the iteration with at least 1 valid heap slot to scan */
	if(_heapSlotCurrent < _heapChunkTop) {
		_heapMapSlotValue = *_heapMapSlotCurrent;
		_heapMapSlotValue >>= _bitIndexHead;
	}

	return true;
}

bool 
MM_HeapMapIterator::reset(MM_HeapMap *heapMap, uintptr_t *heapChunkBase, uintptr_t *heapChunkTop)
{
	uintptr_t heapOffsetInBytes = (uintptr_t)heapChunkBase - (uintptr_t)heapMap->getHeapBase();
	_heapChunkTop = heapChunkTop;
	_heapSlotCurrent = heapChunkBase;
	
	_bitIndexHead = heapMap->getBitIndex((omrobjectptr_t)heapChunkBase);
	
	_heapMapSlotCurrent = (uintptr_t *) ( ((uint8_t *)heapMap->getHeapMapBits())
		+ (MM_Math::roundToFloor(J9MODRON_HMI_HEAPMAP_ALIGNMENT, heapOffsetInBytes) / J9MODRON_HEAP_SLOTS_PER_HEAPMAP_SLOT) );

	/* Cache the first heap map value ONLY if we are starting the iteration with at least 1 valid heap slot to scan */
	if(_heapSlotCurrent < _heapChunkTop) {
		_heapMapSlotValue = *_heapMapSlotCurrent;
		_heapMapSlotValue >>= _bitIndexHead;
	}

	return true;
}

omrobjectptr_t
MM_HeapMapIterator::nextObject()
{
	uintptr_t leadingZeroes = 0;

	/* NOTE: Two overriding goals in this algorithm are:
	 * 1) To avoid looping (hence the use of leadingZeroes())
	 * 2) To avoid fetching memory where possible.
	 * The second goal we accomplish in part by caching the heap map slot value.  This may seem like unnecessary work but in
	 * fact avoids polluting the memory hierarchy and results in a net performance win.  Be aware when changing this code that
	 * you should avoid fetching values from memory (non-local to the stack) as much as possible.
	 */
	while (_heapSlotCurrent < _heapChunkTop) {
		/* By using the heap map slot value and shifting it to examine only the LSB, we are effectively also using a
		 * form of trailingZeroes().
		 */
		if (_heapMapSlotValue != J9MODRON_HMI_SLOT_EMPTY) {
			leadingZeroes = MM_Bits::leadingZeroes(_heapMapSlotValue);

			if(0 != leadingZeroes) {
				_heapSlotCurrent += J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * leadingZeroes;
				_heapMapSlotValue >>= leadingZeroes;
				_bitIndexHead += leadingZeroes;
			}

			omrobjectptr_t nextObject = (omrobjectptr_t)_heapSlotCurrent;
			uintptr_t sizeInHeapMapBits = 1;
			if (_useLargeObjectOptimization) {
				sizeInHeapMapBits = MM_Bits::convertBytesToSlots(_extensions->objectModel.getConsumedSizeInBytesWithHeader(nextObject)) / J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT;
			}

			/* Jump over the body of the object */
			_heapSlotCurrent += J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * sizeInHeapMapBits;
			uintptr_t heapMapAdvance = (_bitIndexHead + sizeInHeapMapBits) / J9BITS_BITS_IN_SLOT;
			_heapMapSlotCurrent += heapMapAdvance;
			_bitIndexHead = ((_bitIndexHead + sizeInHeapMapBits) % J9BITS_BITS_IN_SLOT);

			/* We need to avoid visiting the heap map to refresh the slot if necessary.  Check if the heap map pointer
			 * has been advanced - if it has, we need to load the new value and adjust according to the scan index.
			 * Otherwise just adjust our existing copy.
			 */
			if(0 != heapMapAdvance) {
				if(_heapSlotCurrent < _heapChunkTop) {
					/* Not adjusting the map will be inconsequential - the iteration is complete */
					_heapMapSlotValue = *_heapMapSlotCurrent;
					_heapMapSlotValue >>= _bitIndexHead;
				}
			} else {
				_heapMapSlotValue >>= (sizeInHeapMapBits % J9BITS_BITS_IN_SLOT);
			}

			/* Ensure we don't return an object outside the defined range */
			return (nextObject < (omrobjectptr_t)_heapChunkTop ? nextObject : NULL);
		}

		/* The termination point may not be at the end of the map slot - adjust accordingly */
		_heapSlotCurrent += J9MODRON_HEAP_SLOTS_PER_HEAPMAP_BIT * (J9BITS_BITS_IN_SLOT - _bitIndexHead);

		/* Move to the next mark map slot */
		_heapMapSlotCurrent += 1;
		_bitIndexHead = 0;
		if(_heapSlotCurrent < _heapChunkTop) {
			_heapMapSlotValue = *_heapMapSlotCurrent;
		}
	}

	return (omrobjectptr_t)NULL;
}
