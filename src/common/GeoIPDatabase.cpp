/*
	OpenLieroX

	reader for IpToCountry database
	
	code under LGPL
	by Albert Zeyer and Dark Charlie
*/


#include "GeoIPDatabase.h"
#include "FindFile.h"

//
// Defines
//
typedef enum {
	GEOIP_COUNTRY_EDITION     = 1,
	GEOIP_REGION_EDITION_REV0 = 7,
	GEOIP_CITY_EDITION_REV0   = 6,
	GEOIP_ORG_EDITION         = 5,
	GEOIP_ISP_EDITION         = 4,
	GEOIP_CITY_EDITION_REV1   = 2,
	GEOIP_REGION_EDITION_REV1 = 3,
	GEOIP_PROXY_EDITION       = 8,
	GEOIP_ASNUM_EDITION       = 9,
	GEOIP_NETSPEED_EDITION    = 10,
	GEOIP_DOMAIN_EDITION      = 11,
        GEOIP_COUNTRY_EDITION_V6  = 12,
} GeoIPDBTypes;

#define SEGMENT_RECORD_LENGTH 3
#define STANDARD_RECORD_LENGTH 3
#define ORG_RECORD_LENGTH 4
#define MAX_RECORD_LENGTH 4
#define NUM_DB_TYPES 20
#define FULL_RECORD_LENGTH 50

#define COUNTRY_BEGIN 16776960
#define STATE_BEGIN_REV0 16700000
#define STATE_BEGIN_REV1 16000000
#define STRUCTURE_INFO_MAX_SIZE 20
#define DATABASE_INFO_MAX_SIZE 100
#define MAX_ORG_RECORD_LENGTH 300
#define US_OFFSET 1
#define CANADA_OFFSET 677
#define WORLD_OFFSET 1353
#define FIPS_RANGE 360


//
// Country codes and names
//

const char GeoIP_country_code[253][3] = { "--","AP","EU","AD","AE","AF","AG","AI","AL","AM","AN",
	"AO","AQ","AR","AS","AT","AU","AW","AZ","BA","BB",
	"BD","BE","BF","BG","BH","BI","BJ","BM","BN","BO",
	"BR","BS","BT","BV","BW","BY","BZ","CA","CC","CD",
	"CF","CG","CH","CI","CK","CL","CM","CN","CO","CR",
	"CU","CV","CX","CY","CZ","DE","DJ","DK","DM","DO",
	"DZ","EC","EE","EG","EH","ER","ES","ET","FI","FJ",
	"FK","FM","FO","FR","FX","GA","GB","GD","GE","GF",
	"GH","GI","GL","GM","GN","GP","GQ","GR","GS","GT",
	"GU","GW","GY","HK","HM","HN","HR","HT","HU","ID",
	"IE","IL","IN","IO","IQ","IR","IS","IT","JM","JO",
	"JP","KE","KG","KH","KI","KM","KN","KP","KR","KW",
	"KY","KZ","LA","LB","LC","LI","LK","LR","LS","LT",
	"LU","LV","LY","MA","MC","MD","MG","MH","MK","ML",
	"MM","MN","MO","MP","MQ","MR","MS","MT","MU","MV",
	"MW","MX","MY","MZ","NA","NC","NE","NF","NG","NI",
	"NL","NO","NP","NR","NU","NZ","OM","PA","PE","PF",
	"PG","PH","PK","PL","PM","PN","PR","PS","PT","PW",
	"PY","QA","RE","RO","RU","RW","SA","SB","SC","SD",
	"SE","SG","SH","SI","SJ","SK","SL","SM","SN","SO",
	"SR","ST","SV","SY","SZ","TC","TD","TF","TG","TH",
	"TJ","TK","TM","TN","TO","TL","TR","TT","TV","TW",
	"TZ","UA","UG","UM","US","UY","UZ","VA","VC","VE",
	"VG","VI","VN","VU","WF","WS","YE","YT","RS","ZA",
	"ZM","ME","ZW","A1","A2","O1","AX","GG","IM","JE",
  "BL","MF"};

static const unsigned GeoIP_country_count = (unsigned)(sizeof(GeoIP_country_code)/sizeof(GeoIP_country_code[0]));

const char GeoIP_country_code3[253][4] = { "--","AP","EU","AND","ARE","AFG","ATG","AIA","ALB","ARM","ANT",
	"AGO","AQ","ARG","ASM","AUT","AUS","ABW","AZE","BIH","BRB",
	"BGD","BEL","BFA","BGR","BHR","BDI","BEN","BMU","BRN","BOL",
	"BRA","BHS","BTN","BV","BWA","BLR","BLZ","CAN","CC","COD",
	"CAF","COG","CHE","CIV","COK","CHL","CMR","CHN","COL","CRI",
	"CUB","CPV","CX","CYP","CZE","DEU","DJI","DNK","DMA","DOM",
	"DZA","ECU","EST","EGY","ESH","ERI","ESP","ETH","FIN","FJI",
	"FLK","FSM","FRO","FRA","FX","GAB","GBR","GRD","GEO","GUF",
	"GHA","GIB","GRL","GMB","GIN","GLP","GNQ","GRC","GS","GTM",
	"GUM","GNB","GUY","HKG","HM","HND","HRV","HTI","HUN","IDN",
	"IRL","ISR","IND","IO","IRQ","IRN","ISL","ITA","JAM","JOR",
	"JPN","KEN","KGZ","KHM","KIR","COM","KNA","PRK","KOR","KWT",
	"CYM","KAZ","LAO","LBN","LCA","LIE","LKA","LBR","LSO","LTU",
	"LUX","LVA","LBY","MAR","MCO","MDA","MDG","MHL","MKD","MLI",
	"MMR","MNG","MAC","MNP","MTQ","MRT","MSR","MLT","MUS","MDV",
	"MWI","MEX","MYS","MOZ","NAM","NCL","NER","NFK","NGA","NIC",
	"NLD","NOR","NPL","NRU","NIU","NZL","OMN","PAN","PER","PYF",
	"PNG","PHL","PAK","POL","SPM","PCN","PRI","PSE","PRT","PLW",
	"PRY","QAT","REU","ROU","RUS","RWA","SAU","SLB","SYC","SDN",
	"SWE","SGP","SHN","SVN","SJM","SVK","SLE","SMR","SEN","SOM",
	"SUR","STP","SLV","SYR","SWZ","TCA","TCD","TF","TGO","THA",
	"TJK","TKL","TKM","TUN","TON","TLS","TUR","TTO","TUV","TWN",
	"TZA","UKR","UGA","UM","USA","URY","UZB","VAT","VCT","VEN",
	"VGB","VIR","VNM","VUT","WLF","WSM","YEM","YT","SRB","ZAF",
	"ZMB","MNE","ZWE","A1","A2","O1","ALA","GGY","IMN","JEY",
  "BLM","MAF"};

const char * GeoIP_country_name[253] = {"N/A","Asia/Pacific Region","Europe","Andorra","United Arab Emirates","Afghanistan","Antigua and Barbuda","Anguilla","Albania","Armenia","Netherlands Antilles",
	"Angola","Antarctica","Argentina","American Samoa","Austria","Australia","Aruba","Azerbaijan","Bosnia and Herzegovina","Barbados",
	"Bangladesh","Belgium","Burkina Faso","Bulgaria","Bahrain","Burundi","Benin","Bermuda","Brunei Darussalam","Bolivia",
	"Brazil","Bahamas","Bhutan","Bouvet Island","Botswana","Belarus","Belize","Canada","Cocos (Keeling) Islands","Congo, The Democratic Republic of the",
	"Central African Republic","Congo","Switzerland","Cote D'Ivoire","Cook Islands","Chile","Cameroon","China","Colombia","Costa Rica",
	"Cuba","Cape Verde","Christmas Island","Cyprus","Czech Republic","Germany","Djibouti","Denmark","Dominica","Dominican Republic",
	"Algeria","Ecuador","Estonia","Egypt","Western Sahara","Eritrea","Spain","Ethiopia","Finland","Fiji",
	"Falkland Islands (Malvinas)","Micronesia, Federated States of","Faroe Islands","France","France, Metropolitan","Gabon","United Kingdom","Grenada","Georgia","French Guiana",
	"Ghana","Gibraltar","Greenland","Gambia","Guinea","Guadeloupe","Equatorial Guinea","Greece","South Georgia and the South Sandwich Islands","Guatemala",
	"Guam","Guinea-Bissau","Guyana","Hong Kong","Heard Island and McDonald Islands","Honduras","Croatia","Haiti","Hungary","Indonesia",
	"Ireland","Israel","India","British Indian Ocean Territory","Iraq","Iran, Islamic Republic of","Iceland","Italy","Jamaica","Jordan",
	"Japan","Kenya","Kyrgyzstan","Cambodia","Kiribati","Comoros","Saint Kitts and Nevis","Korea, Democratic People's Republic of","Korea, Republic of","Kuwait",
	"Cayman Islands","Kazakhstan","Lao People's Democratic Republic","Lebanon","Saint Lucia","Liechtenstein","Sri Lanka","Liberia","Lesotho","Lithuania",
	"Luxembourg","Latvia","Libyan Arab Jamahiriya","Morocco","Monaco","Moldova, Republic of","Madagascar","Marshall Islands","Macedonia","Mali",
	"Myanmar","Mongolia","Macau","Northern Mariana Islands","Martinique","Mauritania","Montserrat","Malta","Mauritius","Maldives",
	"Malawi","Mexico","Malaysia","Mozambique","Namibia","New Caledonia","Niger","Norfolk Island","Nigeria","Nicaragua",
	"Netherlands","Norway","Nepal","Nauru","Niue","New Zealand","Oman","Panama","Peru","French Polynesia",
	"Papua New Guinea","Philippines","Pakistan","Poland","Saint Pierre and Miquelon","Pitcairn Islands","Puerto Rico","Palestinian Territory","Portugal","Palau",
	"Paraguay","Qatar","Reunion","Romania","Russian Federation","Rwanda","Saudi Arabia","Solomon Islands","Seychelles","Sudan",
	"Sweden","Singapore","Saint Helena","Slovenia","Svalbard and Jan Mayen","Slovakia","Sierra Leone","San Marino","Senegal","Somalia","Suriname",
	"Sao Tome and Principe","El Salvador","Syrian Arab Republic","Swaziland","Turks and Caicos Islands","Chad","French Southern Territories","Togo","Thailand",
	"Tajikistan","Tokelau","Turkmenistan","Tunisia","Tonga","Timor-Leste","Turkey","Trinidad and Tobago","Tuvalu","Taiwan",
	"Tanzania, United Republic of","Ukraine","Uganda","United States Minor Outlying Islands","United States","Uruguay","Uzbekistan","Holy See (Vatican City State)","Saint Vincent and the Grenadines","Venezuela",
	"Virgin Islands, British","Virgin Islands, U.S.","Vietnam","Vanuatu","Wallis and Futuna","Samoa","Yemen","Mayotte","Serbia","South Africa",
	"Zambia","Montenegro","Zimbabwe","Anonymous Proxy","Satellite Provider","Other","Aland Islands","Guernsey","Isle of Man","Jersey",
  "Saint Barthelemy","Saint Martin"};

/* Possible continent codes are AF, AS, EU, NA, OC, SA for Africa, Asia, Europe, North America, Oceania
and South America. */

const char GeoIP_country_continent[253][3] = {"--","AS","EU","EU","AS","AS","SA","SA","EU","AS","SA",
	"AF","AN","SA","OC","EU","OC","SA","AS","EU","SA",
	"AS","EU","AF","EU","AS","AF","AF","SA","AS","SA",
	"SA","SA","AS","AF","AF","EU","SA","NA","AS","AF",
	"AF","AF","EU","AF","OC","SA","AF","AS","SA","SA",
	"SA","AF","AS","AS","EU","EU","AF","EU","SA","SA",
	"AF","SA","EU","AF","AF","AF","EU","AF","EU","OC",
	"SA","OC","EU","EU","EU","AF","EU","SA","AS","SA",
	"AF","EU","SA","AF","AF","SA","AF","EU","SA","SA",
	"OC","AF","SA","AS","AF","SA","EU","SA","EU","AS",
	"EU","AS","AS","AS","AS","AS","EU","EU","SA","AS",
	"AS","AF","AS","AS","OC","AF","SA","AS","AS","AS",
	"SA","AS","AS","AS","SA","EU","AS","AF","AF","EU",
	"EU","EU","AF","AF","EU","EU","AF","OC","EU","AF",
	"AS","AS","AS","OC","SA","AF","SA","EU","AF","AS",
	"AF","NA","AS","AF","AF","OC","AF","OC","AF","SA",
	"EU","EU","AS","OC","OC","OC","AS","SA","SA","OC",
	"OC","AS","AS","EU","SA","OC","SA","AS","EU","OC",
	"SA","AS","AF","EU","AS","AF","AS","OC","AF","AF",
	"EU","AS","AF","EU","EU","EU","AF","EU","AF","AF",
	"SA","AF","SA","AS","AF","SA","AF","AF","AF","AS",
	"AS","OC","AS","AF","OC","AS","AS","SA","OC","AS",
	"AF","EU","AF","OC","NA","SA","AS","EU","SA","SA",
	"SA","SA","AS","OC","OC","OC","AS","AF","EU","AF",
	"AF","EU","AF","--","--","--","EU","EU","EU","EU",
  "SA","SA"};

//
// GeoRecord operations
//

GeoRecord& GeoRecord::operator= (const GeoRecord& oth)
{
	if (this != &oth)  {
		continentCode = oth.continentCode;
		continent = oth.continent;
		countryCode = oth.countryCode;
		countryCode3 = oth.countryCode3;
		countryName = oth.countryName;
		hasCityLevel = oth.hasCityLevel;
		region = oth.region;
		city = oth.city;
		postalCode = oth.postalCode;
		latitude = oth.latitude;
		longitude = oth.longitude;
		dmaCode = oth.dmaCode;
		areaCode = oth.areaCode;	
	}

	return *this;
}

//
// GeoIP Database class methods
//


/////////////////
// Destructor
GeoIPDatabase::~GeoIPDatabase()
{
	if (m_file)  {
		fclose(m_file);
	}
	if (m_dbSegments)  {
		delete[] m_dbSegments;
	}

	m_file = NULL;
}

////////////////
// Loads the database, returns false on failure
bool GeoIPDatabase::load(const std::string& filename)
{
	m_file = OpenGameFile(filename, "rb");
	if (!m_file)
		return false;

	m_fileName = filename;
	if (!setupSegments())  {
		fclose(m_file);
		return false;
	}

	return true;
}

/////////////////
// Reads database segments (private)
// Requires the database file to be open
// Returns true on success, false otherwise
bool GeoIPDatabase::setupSegments()
{
	// Cleanup
	if (m_dbSegments)
		delete[] m_dbSegments;
	m_dbSegments = NULL;

	// Default to GeoIP Country Edition
	m_dbType = GEOIP_COUNTRY_EDITION;
	m_recordLength = STANDARD_RECORD_LENGTH;
	fseek(m_file, -3l, SEEK_END);
	for (int i = 0; i < STRUCTURE_INFO_MAX_SIZE; i++) {
		unsigned char delim[3];  // Record delimiter
		if (fread(delim, 1, 3, m_file) != 3)
			errors << "Error reading from GeoIP database" << endl;
		if (delim[0] == 255 && delim[1] == 255 && delim[2] == 255) {
			if (fread(&m_dbType, 1, 1, m_file) != 1)
				errors << "Error reading from GeoIP database" << endl;

			// Backwards compatibility with databases from April 2003 and earlier
			if (m_dbType >= 106)
				m_dbType -= 105;

			if (m_dbType == GEOIP_REGION_EDITION_REV0) {
				// Region Edition, pre June 2003
				m_dbSegments = new unsigned int[1];
				m_dbSegments[0] = STATE_BEGIN_REV0;
			} else if (m_dbType == GEOIP_REGION_EDITION_REV1) {
				// Region Edition, post June 2003
				m_dbSegments = new unsigned int[1];
				m_dbSegments[0] = STATE_BEGIN_REV1;
			} else if (m_dbType == GEOIP_CITY_EDITION_REV0 ||
								 m_dbType == GEOIP_CITY_EDITION_REV1 ||
								 m_dbType == GEOIP_ORG_EDITION ||
								 m_dbType == GEOIP_ISP_EDITION ||
								 m_dbType == GEOIP_ASNUM_EDITION) {

				// City/Org Editions have two segments, read offset of second segment
				m_dbSegments = new unsigned int[1];
				m_dbSegments[0] = 0;

				unsigned char buf[SEGMENT_RECORD_LENGTH];
				if (fread(buf, SEGMENT_RECORD_LENGTH, 1, m_file) != 1)
					errors << "Error reading from GeoIP database" << endl;
				for (int j = 0; j < SEGMENT_RECORD_LENGTH; j++)
					m_dbSegments[0] += (buf[j] << (j * 8));
				
				if (m_dbType == GEOIP_ORG_EDITION || m_dbType == GEOIP_ISP_EDITION)
					m_recordLength = ORG_RECORD_LENGTH;
			}
			break;
		} else {
			fseek(m_file, -4l, SEEK_CUR);
		}
	}

	if (m_dbType == GEOIP_COUNTRY_EDITION ||
			m_dbType == GEOIP_PROXY_EDITION ||
			m_dbType == GEOIP_NETSPEED_EDITION ||
			m_dbType == GEOIP_COUNTRY_EDITION_V6 ) {
		m_dbSegments = new unsigned int[1];
		m_dbSegments[0] = COUNTRY_BEGIN;
	}

	return true;
}

////////////////
// Seek a record in the database, returns record index or 0 on failure
unsigned int GeoIPDatabase::seekRecord(unsigned long ipnum) const
{
	if (!m_file)
		return 0;

	unsigned int x;
	unsigned char stack_buffer[2 * MAX_RECORD_LENGTH];
	const unsigned char *buf = stack_buffer;
	unsigned int offset = 0;

	const unsigned char * p;

	for (int depth = 31; depth >= 0; depth--) {
		// Read from disk
		fseek(m_file, (long)m_recordLength * 2 * offset, SEEK_SET);
		if (fread(stack_buffer, m_recordLength, 2, m_file) != 2)
			errors << "Error reading from GeoIP database" << endl;

		if (ipnum & (1 << depth)) {
			// Take the right-hand branch
			if (m_recordLength == 3) {
				// Most common case is completely unrolled and uses constants
				x =   (buf[3*1 + 0] << (0*8))
					+ (buf[3*1 + 1] << (1*8))
					+ (buf[3*1 + 2] << (2*8));

			} else {
				// General case
				int j = m_recordLength;
				p = &buf[2*j];
				x = 0;
				do {
					x <<= 8;
					x += *(--p);
				} while (--j);
			}

		} else {
			// Take the left-hand branch
			if (m_recordLength == 3) {
				// Most common case is completely unrolled and uses constants
				x =   (buf[3*0 + 0] << (0*8))
					+ (buf[3*0 + 1] << (1*8))
					+ (buf[3*0 + 2] << (2*8));
			} else {
				// General case
				int j = m_recordLength;
				p = &buf[1*j];
				x = 0;
				do {
					x <<= 8;
					x += *(--p);
				} while (--j);
			}
		}

		if (x >= m_dbSegments[0])
			return x;

		offset = x;
	}

	// Shouldn't reach here
	errors << "Error Traversing Database for ipnum = %lu - Perhaps database is corrupt?" << endl;
	return 0;
}

//////////////////
// Extracts a record information from a city-level database
GeoRecord GeoIPDatabase::extractRecordCity(unsigned int seekRecord) const
{
	GeoRecord record;

	int record_pointer;
	unsigned char *record_buf = NULL;
	unsigned char *begin_record_buf = NULL;
	double latitude = 0, longitude = 0;
	int metroarea_combo = 0;
	size_t bytes_read = 0;
	if (seekRecord == m_dbSegments[0])		
		return record;

	record_pointer = seekRecord + (2 * m_recordLength - 1) * m_dbSegments[0];

	// Seek to the record
	fseek(m_file, record_pointer, SEEK_SET);
	begin_record_buf = record_buf = new unsigned char[FULL_RECORD_LENGTH];
	bytes_read = fread(record_buf, sizeof(char), FULL_RECORD_LENGTH, m_file);
	if (bytes_read == 0) {
		// Eof or other error
		delete[] begin_record_buf;
		return record;
	}

	// Get country
	record.continentCode = GeoIP_country_continent[record_buf[0]];
	record.countryCode = GeoIP_country_code[record_buf[0]];
	record.countryCode3 = GeoIP_country_code3[record_buf[0]];
	record.countryName = GeoIP_country_name[record_buf[0]];
	record_buf++;

	// Get region
	record.region = (char *)record_buf;
	record_buf += record.region.size() + 1;
	record.region = ISO88591ToUtf8(record.region);

	// Get city
	record.city = (char *)record_buf;
	record_buf += record.city.size() + 1;
	record.city = ISO88591ToUtf8(record.city);


	// Get postal code
	record.postalCode = (char *)record_buf;
	record_buf += record.postalCode.size() + 1;

	// Get latitude
	for (int j = 0; j < 3; ++j)
		latitude += (record_buf[j] << (j * 8));
	record.latitude = latitude/10000 - 180;
	record_buf += 3;

	// Get longitude
	for (int j = 0; j < 3; ++j)
		longitude += (record_buf[j] << (j * 8));
	record.longitude = longitude/10000 - 180;

	// Get area code and metro code for post April 2002 databases and for US locations
	if (GEOIP_CITY_EDITION_REV1 == m_dbType) {
		if (record.countryCode == "US") {
			record_buf += 3;
			for (int j = 0; j < 3; ++j)
				metroarea_combo += (record_buf[j] << (j * 8));
			record.metroCode = metroarea_combo/1000;
			record.areaCode = metroarea_combo % 1000;
		}
	}

	delete[] begin_record_buf;

	record.hasCityLevel = true;
	fillContinent(record);

	return record;
}

///////////////////
// Converts continent codes to full continent name
void GeoIPDatabase::fillContinent(GeoRecord& res) const
{
	// Fill in continent based on continent shortcut
	if (res.continentCode == "EU")
		res.continent = "Europe";
	else if (res.continentCode == "NA")
		res.continent = "North America";
	else if (res.continentCode == "SA")
		res.continent = "South America";
	else if (res.continentCode == "AS")
		res.continent = "Asia";
	else if (res.continentCode == "OC")
		res.continent = "Australia and Oceania";
	else if (res.continentCode == "AN")
		res.continent = "Antarctica";
	else if (res.continentCode == "AF")
		res.continent = "Africa";
	else
		res.continent = "Unknown";
}

///////////////
// Reads a records from a country-only database
GeoRecord GeoIPDatabase::extractRecordCtry(unsigned int seekRecord) const
{
	GeoRecord res;

	int ctry = seekRecord - COUNTRY_BEGIN;

	// seekRecord contains coutry ID in this case
	// But to be sure we check for the range
	if (ctry >= (int)GeoIP_country_count || ctry < 0)
		return res;

	// Fill in the info
	res.continentCode = GeoIP_country_continent[ctry];
	res.countryCode = GeoIP_country_code[ctry];
	res.countryCode3 = GeoIP_country_code3[ctry];
	res.countryName = GeoIP_country_name[ctry];
	res.hasCityLevel = false;

	fillContinent(res);

	return res;
}

//////////////////
// Converts a string IP to MaxMind's representation
unsigned long GeoIPDatabase::convertIp(const std::string& strAddr) const
{
	unsigned int    octet;
	unsigned long   ipnum;
	int             i = 3;

	octet = ipnum = 0;
	for (std::string::const_iterator it = strAddr.begin(); it != strAddr.end(); it++) {
		if (*it == '.') {
			if (octet > 255)
				return 0;
			ipnum <<= 8;
			ipnum += octet;
			i--;
			octet = 0;
		} else if (*it == ':') {
			break;
		} else {
			int t = octet;
			octet <<= 3;
			octet += t;
			octet += t;
			int c = (int)(unsigned char)(*it);
			c -= '0';
			if (c > 9 || c < 0)
				return 0;
			octet += c;
		}
	}
	if ((octet > 255) || (i != 0))
		return 0;
	ipnum <<= 8;
	return ipnum + octet;
}

/////////////////
// Performs a search for the given IP, returns a record with information about the IP
GeoRecord GeoIPDatabase::lookup(const std::string& ip) const
{
	GeoRecord res;

	if (!m_file)
		return res;

	// IP check
	unsigned long l_ip = convertIp(ip);
	if (!l_ip)  {
		res.continentCode = "UN";
		res.countryCode = "HCK";
		res.countryCode3 = "HCK";
		res.countryName = "Hackerland";
		res.region = "Pirate area";
		res.city = "No City";
		return res;
	}

	// Find the record
	int record = seekRecord(l_ip);

	// Get information
	if (m_dbType == GEOIP_CITY_EDITION_REV0 || m_dbType == GEOIP_CITY_EDITION_REV1)
		return extractRecordCity(record);
	else if (m_dbType == GEOIP_COUNTRY_EDITION)
		return extractRecordCtry(record);

	errors << "The Geo IP database has an unsupported format" << endl;
	return res;
}
