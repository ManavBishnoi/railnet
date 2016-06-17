/*************************************************************************/
/* railnet - utilities to export OpenTTD savegames to railroad networks  */
/* Copyright (C) 2016                                                    */
/* Johannes Lorenz (jlsf2013 at sourceforge)                             */
/*                                                                       */
/* This program is free software; you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation; either version 3 of the License, or (at */
/* your option) any later version.                                       */
/* This program is distributed in the hope that it will be useful, but   */
/* WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      */
/* General Public License for more details.                              */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program; if not, write to the Free Software           */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA  */
/*************************************************************************/

#include <map>
#include <iostream>

#include "config.h"
#include "common.h"
#include "railnet2dot_options.h"

void print_version()
{
	std::cerr << "version: <not specified yet>" << std::endl;
	std::cerr << "railnet file version: "
		<< comm::railnet_file_info::_version << std::endl;
}

void print_help()
{
	std::cerr << "railnet2dot" << std::endl;
	std::cerr << "converts openttd railnet files into dot graphs" << std::endl;
	options::usage();
}

enum print_direction_t
{
	pdt_fwd,
	pdt_bwd,
	pdt_both
};

const float large_prime = 7919.0f;
const float large_prime_2 = 5417.0f;
float order_list_step;
float order_list_step_2;

enum station_flag_t
{
	st_used = 1,
	st_passed = 2
};

lbl_conv_t lbl_conv;

template<class Itr>
void print_stations(Itr begin, Itr end, print_direction_t pd,
	const std::map<cargo_label_t, comm::cargo_info>& cargo,
	const std::map<StationID, comm::station_info>& stations,
	const std::vector<std::pair<StationID, bool>>& cur_stations,
	UnitID unit_number,
	float& hue, float& value,
	bool multiple_cargos)
{
	hue = fmod(hue += order_list_step, 1.0f);
	value = fmod(value += order_list_step_2, 1.0f);

//	bool only_double_edges = itr4->second.is_bicycle(); //ol.is_bicycle;
	// TODO: non-bicycles: iterate backwards?
//	std::size_t double_edges = 0;
	std::size_t mid = cur_stations.size() >> 1;
#if 0
	if(!only_double_edges && !(cur_stations.size() & 1))
	{
		for(std::size_t i = 1; i < mid; ++i)
			double_edges += (
				cur_stations[mid+i] == cur_stations[mid-i]);
		only_double_edges = (double_edges == mid - 1);
	}
#endif
	// (FEATURE: fix this for backward orders)
	std::cout << "\t// order " << unit_number << " ("
		<< stations.at(cur_stations[0].first).name.get() << " - "
		<< stations.at(cur_stations[mid].first).name.get()
		<< ")" << std::endl;

	for(Itr itr = begin; itr != end; ++itr)
	{
		std::cout
			<< ((itr == begin) ? "\t// " : " - ")
			<< stations.at(itr->first).name.get()
			<< (itr->second ? "" : "(p)");
	}
	std::cout << std::endl;

	auto print_end = [&](bool double_edge)
	{
		// values below 50 get difficult to read
		float _value = 0.5f + (value/2.0f);
		std::cout << " [color=\"" << hue << ", 1.0, " << _value << "\"";
		if(double_edge)
		{
			std::cout << ", dir=none";
		}
		if(multiple_cargos)
		{
			std::cout << ", label=\"";
			bool first = true;
			for(const std::pair<const CargoLabel, comm::cargo_info>& id : cargo)
			if(((id.second.fwd && !id.second.rev) && (pd == pdt_fwd)) ||
				((!id.second.fwd && id.second.rev) && (pd == pdt_bwd)) ||
				((id.second.fwd && id.second.rev) && (pd == pdt_both)) )
			{
				std::cout << (first ? "" : ", ")
					<< lbl_conv.convert(id.first);
				first = false;
			}
			std::cout << "\"";
		}
		std::cout << "];" << std::endl;
	};

	auto print_station = [&](const char* before, const std::pair<StationID, bool>& pr)
	{
		std::cout << before << (pr.second ? "" : "p") << pr.first;
	};

	enum class edge_type_t
	{
		unique,
		duplicate_first,
		duplicate_further
	};
	edge_type_t last_edge_type = edge_type_t::duplicate_further;

	for(Itr itr = begin + 1; itr != end; ++itr)
	{
		const bool first = itr == begin;

		edge_type_t edge_type = edge_type_t::unique;

		if(pd == pdt_both)
			edge_type = edge_type_t::duplicate_first;
		else
		{
			for(Itr itr2 = begin;
				!(int)edge_type && itr2 != itr;
				++itr2)
				// itr2 < itr => itr2 + 1 is valid
				// itr == begin => for loop is not run => itr - 1 is valid
				if(*itr2 == *itr && (*(itr2 + 1)) == (*(itr - 1)) )
					edge_type = edge_type_t::duplicate_further;

			if(!first)
			for(Itr itr2 = itr + 1;
				!(int)edge_type && itr2 != end; ++itr2)
				// itr2 > itr => itr2 - 1 is valid
				// !first => itr - 1 is valid
				if(*(itr2 - 1) == *itr && *itr2 == (*(itr - 1)) )
				 edge_type = edge_type_t::duplicate_first;

			// both edge types
			// second for loop is skipped
			// => duplicate_further wins
		}

		if(last_edge_type != edge_type_t::duplicate_further && edge_type != last_edge_type)
		 print_end(last_edge_type == edge_type_t::duplicate_first);

		if(edge_type != last_edge_type && edge_type != edge_type_t::duplicate_further)
		 print_station("\t", *(itr - 1));

		if(edge_type != edge_type_t::duplicate_further)
		 print_station(" -> ", *itr);

		if(stations.find(itr->first) == stations.end())
		{
			std::cerr << "Could not find station id " << itr->first << std::endl;
			throw std::runtime_error("invalid station id in order list");
		}

		last_edge_type = edge_type;
	}

	if(last_edge_type != edge_type_t::duplicate_further)
	 print_end(last_edge_type == edge_type_t::duplicate_first);
}

bool draw(const options& opt)
{
	comm::railnet_file_info file;
	comm::json_ifile(std::cin) >> file;

	std::map<StationID, int> station_flags;
	const float passed_offset_x = 0.2, passed_offset_y = 0.2;

	/*
		find out which stations are actually being used...
	*/
	// only set flags
	for(std::list<comm::order_list>::const_iterator itr =
		file.order_lists.get().begin();
		itr != file.order_lists.get().end(); ++itr)
	if(itr->stations.get().size())
	{
		const comm::order_list& ol = *itr;
		for(std::vector<std::pair<StationID, bool> >::const_iterator
				itr = ol.stations.get().begin();
				itr != ol.stations.get().end(); ++itr)
		{
			station_flags[itr->first] |= itr->second ? st_used : st_passed;
		}
	}

	// this is required for the drawing algorithm
	for(std::list<comm::order_list>::const_iterator itr3 =
		file.order_lists.get().begin();
		itr3 != file.order_lists.get().end(); ++itr3)
	{
		// all in all, the whole for loop will not affect the order
		comm::order_list& ol = const_cast<comm::order_list&>(*itr3);
		ol.stations.get().push_back(ol.stations.get().front());
	}

	/*
		draw nodes
	*/
	std::cout <<	"digraph graphname\n" // FEATURE: get savegame name
		"{\n"
		"\tgraph[splines=line];\n"
		"\tnode[label=\"\", size=0.2, width=0.2, height=0.2];\n"
		"\tedge[penwidth=2];\n";

	// draw nodes if stations are used
	for(const auto& pr : file.stations.get())
	{
		if(station_flags.at(pr.first) | st_used) // train stops
			std::cout << "\t" << pr.first << " [xlabel=\""
				<< pr.second.name.get() << "\", pos=\""
				<< (pr.second.x * opt.stretch)
				<< ", "
				<< (pr.second.y * opt.stretch)
				<< "!\"];" << std::endl;
		if(station_flags.at(pr.first) | st_passed)
		// train passes through the station
			std::cout << "\tp" << pr.first << " [pos=\""
				<< ((pr.second.x-passed_offset_x) * opt.stretch)
				<< ", "
				<< ((pr.second.y-passed_offset_y) * opt.stretch)
				<< "!\" size=0.0, width=0.0, height=0.0];" << std::endl;
	}

	float value = 0.0f, hue = 0.0f;
	std::cout.precision(3);

	int total_size = 0;
	for(auto itr3 = file.order_lists.get().begin();
		itr3 != file.order_lists.get().end(); ++itr3)
	{
		total_size += itr3->cargo().size();
	}
	order_list_step = ((float)large_prime) / total_size;
	order_list_step_2 = ((float)large_prime_2) / total_size;

	bool multiple_cargo = file.cargo_names().size() > 1;

	/*
		draw edges
	*/
	for(std::list<comm::order_list>::const_iterator itr3 =
		file.order_lists.get().begin();
		itr3 != file.order_lists.get().end(); ++itr3)
	if(itr3->stations.get().size())
	{
		bool has_fwd = false, has_bwd = false, has_double = false;
		for(std::map<cargo_label_t, comm::cargo_info>::const_iterator itr4 = itr3->cargo().begin();
			itr4 != itr3->cargo().end(); ++itr4)
		{
			has_fwd = has_fwd || (itr4->second.fwd && !itr4->second.rev);
			has_bwd = has_bwd || (!itr4->second.fwd && itr4->second.rev);
			has_double = has_double || (itr4->second.fwd && itr4->second.rev);
		}

#if 0
		std::cout << "\t// cargo: ";
		for(std::set<CargoID>::const_iterator itr2 = itr3->cargo.begin();
			itr2 != itr3->cargo.end(); ++itr2)
				std::cout << " " << +*itr2;
		std::cout << std::endl;
#endif
		const auto& cur_stations = itr3->stations.get();

		if(has_fwd)
		 print_stations(cur_stations.begin(), cur_stations.end(),
			pdt_fwd, itr3->cargo, file.stations, cur_stations,
			itr3->unit_number, hue, value, multiple_cargo);
		if(has_double)
		 print_stations(cur_stations.begin(), cur_stations.end(),
			pdt_both, itr3->cargo, file.stations, cur_stations,
			itr3->unit_number, hue, value, multiple_cargo);
		if(has_bwd)
		 print_stations(cur_stations.rbegin(), cur_stations.rend(),
			pdt_bwd, itr3->cargo, file.stations, cur_stations,
			itr3->unit_number, hue, value, multiple_cargo);
		// TODO: do not pass cur_stations?
	}

	std::cout << '}' << std::endl;

	return true;
}

int main(int argc, char** argv)
{
	try
	{
		setlocale(LC_ALL, "en_US.UTF-8");
		options opt(argc, argv);
		switch(opt.command)
		{
			case options::cmd_print_help:
				print_help();
				break;
			case options::cmd_print_version:
				print_version();
				break;
			default:
				draw(opt);
		}
	} catch(const char* s)
	{
		std::cerr << "Error: " << s << std::endl;
		std::cerr << "Rest of file: ";
		char buf[1024];
		int count = 0;
		while(std::cin.getline(buf, 1024) && ++count < 8) {
			std::cerr << "* " << buf << std::endl;
		}
		std::cerr << "Exiting." << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
