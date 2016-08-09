/** \file SqrlUri.h
 *
 * \author Adam Comley
 *
 * This file is part of libsqrl.  It is released under the MIT license.
 * For more details, see the LICENSE file included with this package.
**/

#ifndef SQRLURI_H
#define SQRLURI_H

#include "sqrl.h"
#include "SqrlString.h"

namespace libsqrl
{
	typedef enum
	{
		SQRL_SCHEME_INVALID = 0,
		SQRL_SCHEME_SQRL,
		SQRL_SCHEME_FILE
	} Sqrl_Scheme;

	class DLL_PUBLIC SqrlUri
	{
	public:
		static SqrlUri *parse( SqrlString *source );
		SqrlUri *release();

		Sqrl_Scheme getScheme();

		/** The Challenge is the full, original URL, or the response body from a previous SQRL action */
		char *getChallenge();
		size_t getChallengeLength();
		void setChallenge( const char *val );

		/** The Hostname (fqdn), and any extension defined by the server.  Used in creating Site Specific Keys */
		char *getSiteKey();
		size_t getSiteKeyLength();

		/** The prefix URL.  Combined with a server's qry= parameter, defines where the client should connect for the next loop.
		* Typically, the FQDN, followed by an optional extension.
		*/
		char *getPrefix();
		size_t getPrefixLength();

		/** The server URL for the next action */
		char *getUrl();
		size_t getUrlLength();
		void setUrl( const char *val );

		/** The 'Server Friendly Name' */
		char* getSFN();
		size_t getSFNLength();

		/** Creates a copy of a SqrlUri object. */
		SqrlUri* copy();

	private:
		SqrlUri();
		~SqrlUri();
		Sqrl_Scheme scheme;
		char *challenge;
		char *siteKey;
		char *prefix;
		char *url;
		char *sfn;
	};
}
#endif // SQRLURI_H
