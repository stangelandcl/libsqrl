/** \file SqrlStorage.h
 *
 * \author Adam Comley
 *
 * This file is part of libsqrl.  It is released under the MIT license.
 * For more details, see the LICENSE file included with this package.
**/

#ifndef SQRLSTORAGE_H
#define SQRLSTORAGE_H

#include <stdint.h>
#include "sqrl.h"

class DLL_PUBLIC SqrlStorage
{
public:
	static SqrlStorage *empty();
	static SqrlStorage *from( SQRL_STRING *buffer );
	static SqrlStorage *from( SqrlUri *uri );

	SqrlStorage *release();

	bool hasBlock(uint16_t blockType);
	bool getBlock(SqrlBlock *block, uint16_t blockType);
	bool putBlock(SqrlBlock *block);
	bool removeBlock(uint16_t blockType);

	bool load(SQRL_STRING *buffer);
	bool load(SqrlUri *uri);

	SQRL_STRING *save(Sqrl_Export etype, Sqrl_Encoding encoding);
	bool save(SqrlUri *uri, Sqrl_Export etype, Sqrl_Encoding encoding);

	void getUniqueId(char *unique_id);

private:
	SqrlStorage();
	~SqrlStorage();
	void *data;
};
#endif // SQRLSTORAGE_H
