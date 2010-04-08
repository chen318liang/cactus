#ifndef CACTUS_END_H_
#define CACTUS_END_H_

#include "cactusGlobals.h"

////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////
//Basic end functions.
////////////////////////////////////////////////
////////////////////////////////////////////////
////////////////////////////////////////////////

/*
 * Constructs the stub end, not attached to any block.
 */
End *end_construct(bool isAttached, Net *net);

/*
 * Copies the end, creating a stub (because the end will not be attached to any block).
 * The isAttached can be selected accordingly.
 * Replaces the net attached to the end with the given
 * 'newNet'.
 */
End *end_copyConstruct(End *end, Net *newNet);

/*
 *	Name of the end.
 */
Name end_getName(End *end);

/*
 * Returns a non zero if the end is oriented positively.
 * The orientation is arbitrary (it is not explicitly with respect to anything else), but is consistent.
 */
bool end_getOrientation(End *end);

/*
 * Gets end in positive orientation.
 */
End *end_getPositiveOrientation(End *end);

/*
 * Returns a reverse strand view of the end (in the opposite orientation).
 */
End *end_getReverse(End *end);

/*
 * Gets the net the end is part of.
 */
Net *end_getNet(End *end);

/*
 * Gets the block the end is on the side of.
 */
Block *end_getBlock(End *end);

/*
 * If the end is a block end returns the other end of the block, maintaining
 * the same orientation as end. If not block end will return NULL (so be careful!)
 */
End *end_getOtherBlockEnd(End *end);

/*
 * Gets the group that the end is part of.
 */
Group *end_getGroup(End *end);

/*
 * Sets the group that the end is part of.
 */
void end_setGroup(End *end, Group *group);

/*
 * Returns the number of caps the end contains.
 */
int32_t end_getInstanceNumber(End *end);

/*
 * Gets an instance using its instance name as a key. Instance name is m of full name n.m.
 */
Cap *end_getInstance(End *end, Name instanceName);

/*
 * Gets the first instance in the end, or NULL if none.
 */
Cap *end_getFirst(End *end);

/*
 * Gets the root cap of the end, if it is set, or returns NULL;
 */
Cap *end_getRootInstance(End *end);

/*
 * Sets the root cap of the end. Will throw an error if the cap
 * is not part of the end, or already has a parent.
 */
void end_setRootInstance(End *end, Cap *cap);

/*
 * Gets an iterator over the caps.
 */
End_InstanceIterator *end_getInstanceIterator(End *end);

/*
 * Gets the next cap from the iterator.
 */
Cap *end_getNext(End_InstanceIterator *iterator);

/*
 * Gets the previous cap from the iterator.
 */
Cap *end_getPrevious(End_InstanceIterator *iterator);

/*
 * Duplicates the iterator.
 */
End_InstanceIterator *end_copyInstanceIterator(End_InstanceIterator *iterator);

/*
 * Destructs the iterator.
 */
void end_destructInstanceIterator(End_InstanceIterator *end);

/*
 * Return non zero if the end represents the end of an block represented in the net of the cap (at this level).
 */
bool end_isBlockEnd(End *end);

/*
 * Return non zero if the end is not a block end, i.e. syntactic sugar.. end_isStubEnd(end) == !end_isBlockEnd(end).
 */
bool end_isStubEnd(End *end);

/*
 * Return non zero if the end is a stub end whose 'dead end' (not actually represented in the API)
 * is connected to the dead-end clique (see the cactus paper).
 */
bool end_isAttached(End *end);

/*
 * The opposite of end_isAttached. Syntactic sugar such that !end_isAttached(end) == end_isFree(end).
 */
bool end_isFree(End *end);

#endif
