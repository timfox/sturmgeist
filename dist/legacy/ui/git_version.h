/**
 * @def MODNAME
 * @brief Mod name
 */
#define MODNAME "legacy"

/**
 * @def MODURL
 * @brief Mod URL
 */
#define MODURL "www.etlegacy.com"

/**
 * @def ETLEGACY_VERSION
 * @brief Version in long format.
 *
 * If the current commit is tagged, then it is just the tag.
 * Otherwise, it consists of three parts separated by dashes:
 * 1. most recent tag
 * 2. commits made since last tagging
 * 3. abbreviated SHA1 of the current commit
 */
#define ETLEGACY_VERSION "v2.83.2-203-g8d10fdb"

/**
 * @def ETLEGACY_VERSION_SHORT
 * @brief Version in short format. Contains only the most recent tag.
 */
#define ETLEGACY_VERSION_SHORT "v2.83.2_dirty"

/**
 * @def ETLEGACY_VERSION_INT
 * @brief Version in integral format.
 */
#define ETLEGACY_VERSION_INT 283020203

/**
* @def PRODUCT_VERSION
* @brief Version data used for windows RC compiler
*/
#define PRODUCT_VERSION 2,83,2,203
#define PRODUCT_VERSION_STR "2.83.2.203"

/**
* @def PRODUCT_BUILD_TIME
* @brief Build time information in UTC
*/
#define PRODUCT_BUILD_TIME "2025-08-13T05:10:29 UTC"

/**
* @def PRODUCT_BUILD_FEATURES
* @brief Build time enabled feature set
*/
#define PRODUCT_BUILD_FEATURES "anticheat, auth, autoupdate, curl, dbms, edv, freetype, gettext, ipv6, irc_client, irc_server, lua, luasql, multiview, ogg_vorbis, openal, pakisolation, png, prestige, rating, renderer1, servermdx, ssl, theora, tracker, windows_console"
