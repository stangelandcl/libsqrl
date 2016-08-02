/** @file user.c

@author Adam Comley

This file is part of libsqrl.  It is released under the MIT license.
For more details, see the LICENSE file included with this package.
*/
#include <new>
#include "sqrl_internal.h"
#include "SqrlUser.h"
#include "SqrlAction.h"
#include "SqrlClient.h"
#include "SqrlCrypt.h"
#include "SqrlEntropy.h"
#include "SqrlActionLock.h"

struct SqrlUserList {
	SqrlUser *user;
	struct SqrlUserList *next;
};

struct SqrlUserList *SQRL_USER_LIST;

int SqrlUser::enscryptCallback( int percent, void *data )
{
	struct Sqrl_User_s_callback_data *cbdata = (struct Sqrl_User_s_callback_data*)data;
	if( cbdata ) {
		int progress = cbdata->adder + (int)((double)percent * cbdata->multiplier);
		if( progress > 100 ) progress = 100;
		if( progress < 0 ) progress = 0;
		if( percent == 100 && progress >= 99 ) progress = 100;
		SqrlClient::getClient()->onProgress(cbdata->action, progress);
		return 0;
	} else {
		return 1;
	}
}

SqrlUser *SqrlUser::create() {
	SqrlUser *user = (SqrlUser*)malloc( sizeof( SqrlUser ) );
	new (user) SqrlUser();
	return user;
}

SqrlUser *SqrlUser::create( const char *buffer, size_t buffer_len ) {
	SqrlUser *user = (SqrlUser*)malloc( sizeof( SqrlUser ) );
	new (user) SqrlUser( buffer, buffer_len );
	return user;
}

SqrlUser *SqrlUser::create( SqrlUri *uri ) {
	SqrlUser *user = (SqrlUser*)malloc( sizeof( SqrlUser ) );
	new (user) SqrlUser();
	return user;
}

void SqrlUser::ensureKeysAllocated()
{
	if( this->keys == NULL ) {
		this->keys = (Sqrl_Keys*)sodium_malloc( sizeof( struct Sqrl_Keys ));
		memset( this->keys, 0, sizeof( struct Sqrl_Keys ));
		FLAG_CLEAR( this->flags, USER_FLAG_MEMLOCKED );
	}
}

SqrlUser* SqrlUser::find( const char *unique_id )
{
	SqrlClient *client = SqrlClient::getClient();
	SqrlUser *user = NULL;
	struct SqrlUserList *l;
	client->userMutex.lock();
	l = SQRL_USER_LIST;
	while( l ) {
		if( l->user && l->user->uniqueIdMatches( unique_id )) {
			user = l->user;
			user->hold();
			break;
		}
		l = l->next;
	}
	client->userMutex.unlock();
	return user;
}

void SqrlUser::initialize()
{
	SqrlClient *client = SqrlClient::getClient();
	SqrlUser::defaultOptions(&this->options);
	this->referenceCount = 1;
	this->referenceCountMutex = new std::mutex();
	struct SqrlUserList *l = (struct SqrlUserList*)calloc(1, sizeof(struct SqrlUserList));
	l->user = this;
	client->userMutex.lock();
	l->next = SQRL_USER_LIST;
	SQRL_USER_LIST = l;
	client->userMutex.unlock();
}

SqrlUser::SqrlUser()
{
	this->initialize();
}

int SqrlUser::countUsers()
{
	SqrlClient *client = SqrlClient::getClient();
	client->userMutex.lock();
    int i = 0;
    struct SqrlUserList *list = SQRL_USER_LIST;
    while( list ) {
        i++;
        list = list->next;
    }
    client->userMutex.unlock();
    return i;
}

void SqrlUser::hold()
{
	SqrlClient *client = SqrlClient::getClient();
	client->userMutex.lock();
	// Make sure the user is still in active memory...
	struct SqrlUserList *c = SQRL_USER_LIST;
	while( c ) {
		if( c->user == this ) {
			this->referenceCountMutex->lock();
			this->referenceCount++;
			this->referenceCountMutex->unlock();
			break;
		}
		c = c->next;
	}
	client->userMutex.unlock();
}

void SqrlUser::release()
{
	SqrlClient *client = SqrlClient::getClient();
	bool shouldFreeThis = false;
	client->userMutex.lock();
	struct SqrlUserList *list = SQRL_USER_LIST;
	if( list == NULL ) {
		// Not saved in memory... Go ahead and release it.
		client->userMutex.unlock();
		shouldFreeThis = true;
		goto END;
	}
	struct SqrlUserList *prev;
	if( list->user == this ) {
		prev = NULL;
	} else {
		prev = list;
		list = NULL;
		while( prev ) {
			if( prev->next && prev->next->user == this ) {
				list = prev->next;
				break;
			}
			prev = prev->next;
		}
	}
	if( list == NULL ) {
		// Not saved in memory... Go ahead and release it.
		client->userMutex.unlock();
		shouldFreeThis = true;
		goto END;
	}
	// Release this reference
	this->referenceCountMutex->lock();
	this->referenceCount--;

	if( this->referenceCount > 0 ) {
		// There are other references... Do not delete.
		this->referenceCountMutex->unlock();
		client->userMutex.unlock();
		goto END;
	}
	// There were no other references... We can delete this.
	shouldFreeThis = true;

	if( prev == NULL ) {
		SQRL_USER_LIST = list->next;
	} else {
		prev->next = list->next;
	}
	free( list );
	client->userMutex.unlock();

END:
	if (shouldFreeThis) {
		delete(this);
	}
}

SqrlUser::~SqrlUser()
{
	if (this->keys != NULL) {
		sodium_mprotect_readwrite(this->keys);
		sodium_free(this->keys);
	}
	delete this->referenceCountMutex;
}

bool SqrlUser::isMemLocked()
{
	if( FLAG_CHECK( this->flags, USER_FLAG_MEMLOCKED )) {
		return true;
	}
	return false;
}

void SqrlUser::memLock()
{
	if( this->keys != NULL ) {
		sodium_mprotect_noaccess( this->keys );
	}
	FLAG_SET( this->flags, USER_FLAG_MEMLOCKED );
}

void SqrlUser::memUnlock()
{
	if( this->keys != NULL ) {
		sodium_mprotect_readwrite( this->keys );
	}
	FLAG_CLEAR( this->flags, USER_FLAG_MEMLOCKED );
}

bool SqrlUser::isHintLocked()
{
	if( this->hint_iterations == 0 ) return false;
	return true;
}

void SqrlUser::hintUnlock( SqrlAction *action,
				char *hint,
				size_t length )
{
	if( hint == NULL || length == 0 ) {
		SqrlClient::getClient()->callAuthenticationRequired(action, SQRL_CREDENTIAL_HINT);
		return;
	}
	if( !action ) return;
	if (action->getUser() != this || ! this->isHintLocked()) {
		return;
	}
	struct Sqrl_User_s_callback_data cbdata;
	cbdata.action = action;
	cbdata.adder = 0;
	cbdata.multiplier = 1;

	SqrlCrypt crypt = SqrlCrypt();
	uint8_t iv[12] = {0};
	crypt.plain_text = this->keys->keys[0];
	crypt.text_len = sizeof( struct Sqrl_Keys ) - KEY_SCRATCH_SIZE;
	crypt.salt = this->keys->scratch;
	crypt.iv = iv;
	crypt.tag = this->keys->scratch + 16;
	crypt.cipher_text = this->keys->scratch + 64;
	crypt.add = NULL;
	crypt.add_len = 0;
	crypt.nFactor = SQRL_DEFAULT_N_FACTOR;
	crypt.count = this->hint_iterations;
	crypt.flags = SQRL_DECRYPT | SQRL_ITERATIONS;

	uint8_t *key = this->keys->scratch + 32;
	if( !crypt.genKey( action, hint, length ) ||
		!crypt.doCrypt() ) {
		sodium_memzero( crypt.plain_text, crypt.text_len );
	}
	this->hint_iterations = 0;
	sodium_memzero( key, SQRL_KEY_SIZE );
	sodium_memzero( this->keys->scratch, KEY_SCRATCH_SIZE );
}

static void bin2rc( char *buf, uint8_t *bin ) {
	// bin must be 512+ bits of entropy!
	int i, j, k;
	uint64_t *tmp = (uint64_t*)bin;
	for( i = 0, j = 0; i < 3; i++ ) {
		for( k = 0; k < 8; k++ ) {
			buf[j++] = '0' + (tmp[k] % 10);
			tmp[k] /= 10;
		}
	}
	buf[j] = 0;
}

bool SqrlUser::_keyGen( SqrlAction *action, int key_type, uint8_t *key )
{
	if( !action ) return false;
	if( action->getUser() != this ) {
		return false;
	}
	bool retVal = false;
	int i;
	uint8_t *temp[4];
	int keys[] = {KEY_PIUK0, KEY_PIUK1, KEY_PIUK2, KEY_PIUK3};
	switch( key_type ) {
	case KEY_IUK:
		for( i = 0; i < 4; i++ ) {
			if( this->hasKey( keys[i] )) {
				temp[i] = this->key( action, keys[i] );
			} else {
				temp[i] = this->newKey( keys[i] );
			}
		}
		memcpy( temp[3], temp[2], SQRL_KEY_SIZE );
		memcpy( temp[2], temp[1], SQRL_KEY_SIZE );
		memcpy( temp[1], temp[0], SQRL_KEY_SIZE );
		memcpy( temp[0], key, SQRL_KEY_SIZE );
		SqrlEntropy::bytes( key, SQRL_KEY_SIZE );
		retVal = true;
		break;
	case KEY_MK:
		if( this->hasKey( KEY_IUK )) {
			temp[0] = this->key( action, KEY_IUK );
			if( temp[0] ) {
				SqrlCrypt::generateMasterKey( key, temp[0] );
				retVal = true;
			}
		}
		break;
	case KEY_ILK:
		temp[0] = this->key( action, KEY_IUK );
		if( temp[0] ) {
			SqrlCrypt::generateIdentityLockKey( key, temp[0] );
			retVal = true;
		}
		break;
	case KEY_LOCAL:
		temp[0] = this->key( action, KEY_MK );
		if( temp[0] ) {
			SqrlCrypt::generateLocalKey( key, temp[0] );
			retVal = true;
		}
		break;
	case KEY_RESCUE_CODE:
		temp[0] = (uint8_t*)malloc( 512 );
		if( temp[0] ) {
			memset( key, 0, SQRL_KEY_SIZE );
			sodium_mlock( temp[0], 512 );
			SqrlEntropy::get( temp[0], SQRL_ENTROPY_NEEDED );
			bin2rc( (char*)key, temp[0] );
			sodium_munlock( temp[0], 512 );
			free( temp[0] );
			temp[0] = NULL;
			retVal = true;
		}
		break;
	}
	return retVal;
}

bool SqrlUser::regenKeys( SqrlAction *action )
{
	if( !action ) return false;
	if( action->getUser() != this ) {
		return false;
	}
	uint8_t *key;
	int keys[] = { KEY_MK, KEY_ILK, KEY_LOCAL };
	int i;
	for( i = 0; i < 3; i++ ) {
		key = this->newKey( keys[i] );
		this->_keyGen( action, keys[i], key );
	}
	return true;
}

bool SqrlUser::rekey( SqrlAction *action )
{
	if( !action ) return false;
	if( action->getUser() != this ) {
		return false;
	}
	this->ensureKeysAllocated();
	bool retVal = true;
	uint8_t *key;
	if( this->hasKey( KEY_IUK )) {
		key = this->key( action, KEY_IUK );
	} else {
		key = this->newKey( KEY_IUK );
	}
	if( ! this->_keyGen( action, KEY_IUK, key )) {
		goto ERR;
	}
	key = this->newKey( KEY_RESCUE_CODE );
	if( ! this->_keyGen( action, KEY_RESCUE_CODE, key )) {
		goto ERR;
	}
	if( ! this->regenKeys( action )) {
		goto ERR;
	}
	this->flags |= (USER_FLAG_T1_CHANGED | USER_FLAG_T2_CHANGED);
	goto DONE;

ERR:
	retVal = false;

DONE:
	return retVal;
}

uint8_t *SqrlUser::newKey( int key_type )
{
	int offset = -1;
	int empty = -1;
	int i = 0;
	for( i = 0; i < USER_MAX_KEYS; i++ ) {
		if( this->lookup[i] == key_type ) {
			offset = i;
		}
		if( this->lookup[i] == 0 ) {
			empty = i;
		}
	}
	if( offset == -1 ) {
		// Not Found
		if( empty > -1 ) {
			// Create new slot
			this->lookup[empty] = key_type;
			offset = empty;
		}
	}
	if( offset ) {
		uint8_t *key = this->keys->keys[offset];
		sodium_memzero( key, SQRL_KEY_SIZE );
		return key;
	}
	return NULL;
}

uint8_t *SqrlUser::key( SqrlAction *action, int key_type )
{
	if( !action ) return NULL;
	if( action->getUser() != this ) {
		return NULL;
	}
	int offset, i;
	int loop = -1;
	uint8_t *key;
LOOP:
	loop++;
	if( loop == 3 ) {
		goto DONE;
	}
	offset = -1;
	for( i = 0; i < USER_MAX_KEYS; i++ ) {
		if( this->lookup[i] == key_type ) {
			offset = i;
			break;
		}
	}
	if( offset > -1 ) {
		key = this->keys->keys[offset];
		return key;
	} else {
		// Not Found!
		switch( key_type ) {
		case KEY_RESCUE_CODE:
			// We cannot regenerate this key!
			return NULL;
		case KEY_IUK:
			this->tryLoadRescue(action, true);
			goto LOOP;
			break;
		case KEY_MK:
		case KEY_ILK:
		case KEY_PIUK0:
		case KEY_PIUK1:
		case KEY_PIUK2:
		case KEY_PIUK3:
			this->tryLoadPassword(action, true);
			goto LOOP;
			break;
		}
	}

DONE:
	return NULL;
}

bool SqrlUser::hasKey( int key_type )
{
	int i;
	for( i = 0; i < USER_MAX_KEYS; i++ ) {
		if( this->lookup[i] == key_type ) {
			return true;
		}
	}
	return false;
}

void SqrlUser::removeKey( int key_type )
{
	int offset = -1;
	int i;
	for( i = 0; i < USER_MAX_KEYS; i++ ) {
		if( this->lookup[i] == key_type ) {
			offset = i;
		}
	}
	if( offset > -1 ) {
		sodium_memzero( this->keys->keys[offset], SQRL_KEY_SIZE );
		this->lookup[offset] = 0;
	}
}

char *SqrlUser::getRescueCode( SqrlAction *action )
{
	if( !action ) return NULL;
	if( action->getUser() != this || !this->hasKey( KEY_RESCUE_CODE )) {
		printf( "No key!\n" );
		return NULL;
	}
	char *retVal = (char*)(this->key( action, KEY_RESCUE_CODE ));
	return retVal;
}

bool SqrlUser::setRescueCode( char *rc )
{
	if( strlen( rc ) != 24 ) return false;
	int i;
	for( i = 0; i < SQRL_RESCUE_CODE_LENGTH; i++ ) {
		if( rc[i] < '0' || rc[i] > '9' ) {
			return false;
		}
	}
	uint8_t *key = this->newKey( KEY_RESCUE_CODE );
	memcpy( key, rc, SQRL_RESCUE_CODE_LENGTH );
	return true;
}

bool SqrlUser::forceDecrypt( SqrlAction *t )
{
	if( !t ) return false;
	if( this->key( t, KEY_MK )) {
		return true;
	}
	return false;
}

bool SqrlUser::forceRescue( SqrlAction *t )
{
	if( !t ) return false;
	if( this->key( t, KEY_IUK )) {
		return true;
	}
	return false;
}

size_t SqrlUser::getPasswordLength()
{
	if (this->isHintLocked()) return 0;
	return this->keys->password_len;
}

bool SqrlUser::setPassword( const char *password, size_t password_len )
{
	if( this->isHintLocked() ) return false;
	char *p = this->keys->password;
	size_t *l = &this->keys->password_len;
	if( !p || !l ) {
		return false;
	}
	sodium_memzero( p, KEY_PASSWORD_MAX_LEN );
	if( password_len > KEY_PASSWORD_MAX_LEN ) password_len = KEY_PASSWORD_MAX_LEN;
	memcpy( p, password, password_len );
	if( *l > 0 ) {
		// 	Changing password
		FLAG_SET(this->flags, USER_FLAG_T1_CHANGED);
	}
	*l = password_len;
	return true;
}

uint8_t *SqrlUser::scratch()
{
	this->ensureKeysAllocated();
	return this->keys->scratch;
}

uint8_t SqrlUser::getHintLength()
{
	uint8_t retVal = 0;
	retVal = this->options.hintLength;
	return retVal;
}

uint8_t SqrlUser::getEnscryptSeconds()
{
	uint8_t retVal = 0;
	retVal = this->options.enscryptSeconds;
	return retVal;
}

uint16_t SqrlUser::getTimeoutMinutes()
{
	uint16_t retVal = 0;
	retVal = this->options.timeoutMinutes;
	return retVal;
}

void SqrlUser::setHintLength( uint8_t length )
{
	this->options.hintLength = length;
	FLAG_SET( this->flags, USER_FLAG_T1_CHANGED );
}

void SqrlUser::setEnscryptSeconds( uint8_t seconds )
{
	this->options.enscryptSeconds = seconds;
	FLAG_SET( this->flags, USER_FLAG_T1_CHANGED );
}

void SqrlUser::setTimeoutMinutes( uint16_t minutes )
{
	this->options.timeoutMinutes = minutes;
	FLAG_SET( this->flags, USER_FLAG_T1_CHANGED );
}

uint16_t SqrlUser::getFlags()
{
	return this->options.flags;
}

uint16_t SqrlUser::checkFlags( uint16_t flags )
{
	uint16_t retVal = 0;
	retVal = this->options.flags & flags;
	return retVal;
}

void SqrlUser::setFlags( uint16_t flags )
{
	if( (this->options.flags & flags) != flags ) {
		this->options.flags |= flags;
		FLAG_SET( this->flags, USER_FLAG_T1_CHANGED );
	}
}

void SqrlUser::clearFlags( uint16_t flags )
{
	if( (this->flags & flags) != 0 ) {
		this->options.flags &= ~flags;
		FLAG_SET( this->flags, USER_FLAG_T1_CHANGED );
	}
}

void SqrlUser::defaultOptions( Sqrl_User_Options *options ) {
	options->flags = SQRL_DEFAULT_FLAGS;
	options->hintLength = SQRL_DEFAULT_HINT_LENGTH;
	options->enscryptSeconds = SQRL_DEFAULT_ENSCRYPT_SECONDS;
	options->timeoutMinutes = SQRL_DEFAULT_TIMEOUT_MINUTES;
}

bool SqrlUser::getUniqueId( char *buffer )
{
	if( !buffer ) return false;
	strncpy_s( buffer, SQRL_UNIQUE_ID_LENGTH + 1, this->uniqueId, SQRL_UNIQUE_ID_LENGTH );
	return true;
}

bool SqrlUser::uniqueIdMatches( const char *unique_id )
{
	bool retVal = false;
	if( unique_id == NULL ) {
		if( this->uniqueId[0] == 0 ) {
			retVal = true;
		}
	} else {
		if( 0 == strcmp( unique_id, this->uniqueId )) {
			retVal = true;
		}
	}
	return retVal;
}

