#include "pch.h"

#include <boost/property_tree/xml_parser.hpp>

void SaveProperty(void)
{
	boost::property_tree::wptree root_pt;
	root_pt.put(L"test", 123);
	boost::property_tree::write_xml("aa.xml", root_pt);

}