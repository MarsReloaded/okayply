#pragma once

// ----------------------------------------------
// 
// okayply - An okay ply C++ reader/writer
// 
// Alpha Version 1.0
// 
// license: UNLICENSED
// 
// Copyright(C) 2025 MarcusTU
// 
// This file is part of the okayply project.
// 
// The okayply project cannot be copied, modified and/or distributed
// without the express permission of MarcusTU. You are allowed to
// admire the code though.
// 
// ----------------------------------------------

#include <unordered_map>
#include <string_view>
#include <functional>
#include <typeindex>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <array>
#include <span>
#include <any>

#define USE_CRLF_LFCR_HEADER_HACK

namespace okayply {

	enum class format : std::uint8_t { ascii, binary };
	struct property;
	struct element;
	struct root;
	struct io;

	namespace {
		template<typename T> using vec = std::vector<T>;
		namespace str {
			inline constexpr auto t_char = "char";
			inline constexpr auto t_int8 = "int8";
			inline constexpr auto t_uchar = "uchar";
			inline constexpr auto t_uint8 = "uint8";
			inline constexpr auto t_short = "short";
			inline constexpr auto t_int16 = "int16";
			inline constexpr auto t_ushort = "ushort";
			inline constexpr auto t_uint16 = "uint16";
			inline constexpr auto t_int = "int";
			inline constexpr auto t_int32 = "int32";
			inline constexpr auto t_uint = "uint";
			inline constexpr auto t_uint32 = "uint32";
			inline constexpr auto t_float = "float";
			inline constexpr auto t_float32 = "float32";
			inline constexpr auto t_double = "double";
			inline constexpr auto t_float64 = "float64";
			inline constexpr auto element = "element";
			inline constexpr auto property = "property";
			inline constexpr auto list = "list";
			inline constexpr auto comment = "comment";
			inline constexpr auto ply = "ply";
			inline constexpr auto format = "format";
			inline constexpr auto end_header = "end_header";
			inline constexpr auto ascii = "ascii";
			inline constexpr auto binary_little_endian = "binary_little_endian";
			inline constexpr auto binary_big_endian = "binary_big_endian";
			inline constexpr auto version = "1.0";
			inline constexpr auto cr = '\r'; // definition says: use \r but most implementations use \n
			inline constexpr auto lf = '\n'; // definition says: use \r but most implementations use \n
			inline constexpr auto space = ' ';
			inline constexpr auto invalidSymbols = "\n\v\f\r"; // do not use these in comments or names
			inline constexpr auto f32fmt = "{:.9}";
			inline constexpr auto f64fmt = "{:.17}";
		}
		template<format ff, std::endian ee>
		inline constexpr std::string_view formatName() {
			if constexpr (ff == format::ascii)
				return str::ascii;
			if constexpr (ff == format::binary && ee == std::endian::little)
				return str::binary_little_endian;
			if constexpr (ff == format::binary && ee == std::endian::big)
				return str::binary_big_endian;
		}
		inline constexpr std::string_view listIndexTypeName(std::uint8_t i) {
			switch (i) {
			case 1: return str::t_uchar;
			case 2: return str::t_ushort;
			case 4: return str::t_uint;
			default: throw std::runtime_error("invalid list index type");
			}
		}
		template<std::uint8_t i>
		using uintX = std::conditional_t<i == 1, std::uint8_t, std::conditional_t<i == 2,
			std::uint16_t, std::conditional_t<i == 4, std::uint32_t, std::uint64_t>>>;
		struct InfoBase {
			virtual bool isList() const = 0;
			virtual std::type_index tid() const = 0;
			virtual std::type_index listTid() const = 0;
			virtual std::uint8_t listIndexTypeSize(std::any const & any) const = 0;
			virtual void* ptr(std::any & any) const = 0;
			virtual const void* ptr(std::any const& any) const = 0;
			virtual ~InfoBase() = default;
		};
		template<typename T, bool l>
		struct Info : public InfoBase {
			std::type_index listTid() const override { return typeid(vec<T>); }
			std::type_index tid() const override { return typeid(T); }
			bool isList() const override { return l; }
			std::uint8_t listIndexTypeSize(std::any const & any) const override {
				if constexpr (l) {
					auto const& vv = std::any_cast<vec<vec<T>>const&>(any);
					std::size_t max = 0;
					for (auto& v : vv)
						max = std::max(max, v.size());
					return max < 256 ? 1 : max < 65536 ? 2 : 3;
				}
				else if constexpr (!l) {
					return 0;
				}
			}
			void* ptr(std::any & any) const override {
				// It depends on the std::any implementation if this is needed. If
				// sizeof(vec<T>) fits into the small storage of std::any,
				// the adresses of &vec<T> and &std::any are equal. If it
				// does not fit, the vector gets heap allocated and has a different adresss.
				if constexpr (l) return reinterpret_cast<void*>(&std::any_cast<vec<vec<T>>&>(any));
				else             return reinterpret_cast<void*>(&std::any_cast<vec<T>&>(any));
			}
			const void* ptr(std::any const& any) const override {
				if constexpr (l) return reinterpret_cast<const void*>(&std::any_cast<vec<vec<T>>const&>(any));
				else             return reinterpret_cast<const void*>(&std::any_cast<vec<T>const&>(any));
			}
		};
		inline std::uint32_t getline(std::istream& in, std::string& line, char & c) {
			// getline with \r and \n (or both) as line seperators
			// and without leading and trailing whitespace
			// and without empty lines
			// and with tracking of \r and \n usage
			std::uint32_t result = 0;
			line.clear();
			while (in.get(c)) {
				if (c == str::cr || c == str::lf) {
					result += c == str::cr ? 0x00010000 : 0x00000001; // track how often cr and lf are used
					std::size_t start = 0;
					while (start < line.size() && line[start] == str::space) start++;
					std::size_t end = line.size();
					while (end > start && line[end-1] == str::space) end--;
					line = line.substr(start, end - start);
					if (!line.size()) continue;
					return result;
				}
				line += c;
			}
			return 0;
		}
		inline vec<std::string> split(std::string const& s, char delim) {
			vec<std::string> r;
			std::stringstream ss(s);
			std::string token;
			while (std::getline(ss, token, delim))
				r.push_back(token);
			return r;
		}
	}

	// Custom data types can be registered via
	// template<typename T, bool isList> struct CustomIO : public IO
	// and registerType<type, CustomIO>();
	// see IO_X later in the code.
	struct io {
		// Serialize & Deserialize
		virtual void ascI(std::istream&, void*, std::size_t) const = 0;
		virtual void ascO(std::ostream&, const void*, std::size_t) const = 0;
		virtual void binI(std::istream&, void*, std::size_t, std::uint8_t, bool) const = 0;
		virtual void binO(std::ostream&, const void*, std::size_t, std::uint8_t, bool) const = 0;

		// First name will be used when writing, all names are valid for reading
		virtual vec<std::string_view> names() const = 0;

		// Dont touch
		virtual ~io() = default;
	};

	struct property
	{
		friend root;
		friend element;
		std::size_t size() const; // how many datapoints are in the property?
		std::string_view name() const; // whats my name?
		template<typename T> std::span<T> get(); // mutable data access
		std::type_index type() const; // gets the type of the property
		std::type_index listType() const; // for lists, this is vec<type>, for non lists, this is equal to type()
		bool isList() const; // is true if the property is a property list
	private:
		std::any any_;
		element* parent_ = nullptr;
		std::type_index tid_ = typeid(void);
		std::uint8_t listIndexSize_ = 0; // only used when reading files
	};

	struct element
	{
		friend root;
		friend property;
		property& operator()(std::string_view, std::type_index const&); // full initialisation
		property& operator()(std::string_view); // partial initialisation (full after first Property.get<T>())
		std::size_t size() const;
		std::string_view name() const;
		vec<std::reference_wrapper<property>> properties();
	private:
		template<format ff = format::ascii, std::endian ee = std::endian::native>
		void read(std::istream&);
		template<format ff = format::ascii, std::endian ee = std::endian::native>
		void write(std::ostream&) const;
		std::unordered_map<const property*, std::string> names_;
		vec<std::string> order_;
		root* parent_ = nullptr;
		std::unordered_map<std::string, property> properties_;
		std::size_t size_ = 0;
	};

	struct root
	{
		friend element;
		friend property;
		root();
		element& operator()(std::string_view, std::size_t); // full init
		element& operator()(std::string_view); // partial init
		element const& operator()(std::string_view) const;
		vec<std::string>& comments();
		void read(std::istream&);
		void read(std::string const&);
		template<typename T, template<typename, bool> typename CustomIO> void registerType();
		template<format ff = format::ascii, std::endian ee = std::endian::native>
		void write(std::ostream&) const;
		template<format ff = format::ascii, std::endian ee = std::endian::native>
		void write(std::string const&) const;
		std::string str() const;
		char lineSeperator(char);
		vec<std::reference_wrapper<element>> elements();
	private:
		std::type_index typeidFromStr(std::string_view, bool);
		std::unordered_map<std::type_index, std::function<std::any(std::size_t)>> anyvec_;
		vec<std::string> comments_;
		std::unordered_map<std::string, element> elements_;
		std::unordered_map<std::type_index, std::unique_ptr<InfoBase>> info_;
		std::unordered_map<std::type_index, std::unique_ptr<io>> ios_;
		std::unordered_map<const element*, std::string> names_;
		vec<std::string> order_;
		char linesep_ = str::lf;
	};

	// ---------------------------------------------------------------
	// Property
	// ---------------------------------------------------------------

	vec<std::reference_wrapper<property>> element::properties() {
		vec<std::reference_wrapper<property>> info;
		for (auto& [n, p] : properties_)
			info.emplace_back(p);
		return info;
	}

	std::size_t property::size() const { return parent_->size_; }
	std::string_view property::name() const { return parent_->names_.at(this); }
	template<typename T> std::span<T> property::get() {
		if (tid_ != typeid(T)) { // late initialization
			if (tid_ == typeid(void)) {
				tid_ = typeid(T);
				auto& f = parent_->parent_->anyvec_[tid_];
				if (!f) throw std::runtime_error(std::format("Unknown type: \"{}\" (size = {})", typeid(T).name(), sizeof(T)));
				any_ = f(parent_->size_);
			}
			else
				throw std::runtime_error(std::format("Property::get<{}> is incompatible to the stored type \"{}\"", typeid(T).name(), tid_.name()));
		}
		return std::any_cast<vec<T> &>(any_);
	}
	std::type_index property::listType() const { return tid_; }
	std::type_index property::type() const { return parent_->parent_->info_[tid_]->tid(); }
	bool property::isList() const {
		if (tid_ == typeid(void))
			throw std::runtime_error("Property::isList cannot be used before type initialization");
		return parent_->parent_->info_[tid_]->isList();
	};

	// ---------------------------------------------------------------
	// Element
	// ---------------------------------------------------------------

	property& element::operator()(std::string_view name) {
		auto [it, inserted] = properties_.try_emplace(std::string(name));
		if (inserted) {
			order_.push_back(std::string(name));
			names_[&it->second] = std::string(name);
			it->second.parent_ = this;
		}
		return it->second;
	}

	property& element::operator()(std::string_view name, std::type_index const& tid) {
		if (!parent_->ios_.contains(tid))
			throw std::runtime_error(std::format("No IO defined for type \"{}\"", tid.name()));
		auto [it, inserted] = properties_.try_emplace(std::string(name));
		if (inserted) {
			order_.push_back(std::string(name));
			names_[&it->second] = std::string(name);
			it->second.parent_ = this;
			it->second.any_ = parent_->anyvec_[tid](size_);
			it->second.tid_ = tid;
		}
		return it->second;
	}

	std::size_t element::size() const { return size_; }
	std::string_view element::name() const { return parent_->names_.at(this); }

	template<format ff, std::endian ee>
	void element::write(std::ostream& out) const {
		std::size_t np = order_.size();
		vec<const io*> ios(np); // serializer & deserializer for each property
		vec<const void*> ptrs(np); // ptr on the vectors (NOT the data)
		vec<std::uint8_t> lsiz(np); // list index type sizes
		for (std::size_t pIdx = 0; pIdx < np; pIdx++) {
			auto const& p = properties_.at(order_[pIdx]);
			ios[pIdx] = parent_->ios_[p.tid_].get();
			auto& info = *parent_->info_[p.tid_].get();
			ptrs[pIdx] = info.ptr(p.any_);
			lsiz[pIdx] = info.listIndexTypeSize(p.any_);
		}
		if constexpr (ff == format::ascii) {
			for (std::size_t i = 0; i < size_; i++) {
				for (std::size_t pIdx = 0; pIdx < np; pIdx++) {
					ios[pIdx]->ascO(out, ptrs[pIdx], i);
					if (pIdx < np - 1) out << " ";
				}
				if (i < size_ - 1) out << parent_->linesep_;
			}
		}
		else if constexpr (ff == format::binary) {
			for (std::size_t i = 0; i < size_; i++)
				for (std::size_t pIdx = 0; pIdx < np; pIdx++)
					ios[pIdx]->binO(out, ptrs[pIdx], i, lsiz[pIdx], ee != std::endian::native);
		}
		else
			throw std::runtime_error("Unknown output format");
	}

	template<format ff, std::endian ee>
	void element::read(std::istream& in) {
		std::size_t np = order_.size();
		vec<const io*> ios(np); // serializer & deserializer for each property
		vec<void*> ptrs(np); // ptr on the vectors (NOT the data)
		vec<std::uint8_t> lsiz(np); // list index type sizes
		for (std::size_t pIdx = 0; pIdx < np; pIdx++) {
			auto& p = properties_[order_[pIdx]];
			ios[pIdx] = parent_->ios_[p.tid_].get();
			auto& info = *parent_->info_[p.tid_].get();
			ptrs[pIdx] = info.ptr(p.any_);
			lsiz[pIdx] = p.listIndexSize_;
		}
		if constexpr (ff == format::ascii) {
			for (std::size_t i = 0; i < size_; i++)
				for (std::size_t pIdx = 0; pIdx < np; pIdx++)
					ios[pIdx]->ascI(in, ptrs[pIdx], i);
		}
		else if constexpr (ff == format::binary) {
			for (std::size_t i = 0; i < size_; i++)
				for (std::size_t pIdx = 0; pIdx < np; pIdx++)
					ios[pIdx]->binI(in, ptrs[pIdx], i, lsiz[pIdx], ee != std::endian::native);
		}
	}

	// ---------------------------------------------------------------
	// Data
	// ---------------------------------------------------------------

	vec<std::reference_wrapper<element>> root::elements() {
		vec<std::reference_wrapper<element>> info;
		for (auto& [n, e] : elements_)
			info.emplace_back(e);
		return info;
	}

	char root::lineSeperator(char newLinesep) {
		std::swap(linesep_, newLinesep);
		return newLinesep;
	}

	vec<std::string>& root::comments() { return comments_; }
	element& root::operator()(std::string_view name, std::size_t size) {
		auto [it, inserted] = elements_.try_emplace(std::string(name));
		if (inserted) {
			order_.push_back(std::string(name));
			names_[&it->second] = std::string(name);
			it->second.size_ = size;
			it->second.parent_ = this;
		}
		return it->second;
	}
	element& root::operator()(std::string_view name) {
		if (!elements_.contains(std::string(name)))
			throw std::runtime_error("cannot access non initialized element");
		// this could be partial initialization without size, but then the size needs to come from "somewhere"...
		return elements_[std::string(name)];
	}
	element const& root::operator()(std::string_view name) const {
		if (!elements_.contains(std::string(name)))
			throw std::runtime_error("cannot access non initialized element");
		// this could be partial initialization without size, but then the size needs to come from "somewhere"...
		return elements_.at(std::string(name));
	}

	template<typename T, template<typename, bool> typename CustomIO> void root::registerType() {
		ios_.insert({ typeid(T),std::make_unique<CustomIO<T, false>>() });
		info_.insert({ typeid(T),std::make_unique<Info<T, false>>() });
		anyvec_.insert({ typeid(T), [](std::size_t s) { return vec<T>(s); } });
		ios_.insert({ typeid(vec<T>),std::make_unique<CustomIO<T, true>>() });
		info_.insert({ typeid(vec<T>),std::make_unique<Info<T, true>>() });
		anyvec_.insert({ typeid(vec<T>), [](std::size_t s) { return vec<vec<T>>(s); } });
	}

	std::type_index root::typeidFromStr(std::string_view s, bool isList) {
		for (auto const & [tid, ios] : ios_)
			for (auto& sw : ios->names())
				if (sw == s) return isList ? info_[tid]->listTid() : tid;
		return typeid(void);
	}

	void root::read(std::string const& path) {
		std::ifstream in(path, std::ios::binary | std::ios::in);
		read(in);
	}

	void root::read(std::istream& in) {
		comments_.clear();
		elements_.clear();
		names_.clear();
		order_.clear();
		std::string line;
		std::size_t lIdx = 0;
		std::uint32_t crlfCounter = 0;
		char lastDecodedChar;
		format fmt = format::ascii;
		std::endian endian = std::endian::native;
		element* lastElement = nullptr;
		while (true) {
			crlfCounter += getline(in, line, lastDecodedChar);
			if (!line.size() || line == str::end_header)
				break;
			if (lIdx == 0) {
				if (!line.starts_with(str::ply))
					throw std::runtime_error(std::format("read line {}: wrong magic number in ply file", lIdx + 1));
			}
			else if (lIdx == 1) {
				auto a = split(line, str::space);
				if(a.size() != 3 || a[0] != str::format)
					throw std::runtime_error(std::format("read line {}: invalid", lIdx));
				if (a[1] == str::ascii)
					fmt = format::ascii;
				else if (a[1] == str::binary_big_endian)
				{
					fmt = format::binary;
					endian = std::endian::big;
				}
				else if (a[1] == str::binary_little_endian)
				{
					fmt = format::binary;
					endian = std::endian::little;
				}
				else
					throw std::runtime_error(std::format("read line {}: invalid", lIdx));
				if(a[2] != str::version)
					throw std::runtime_error(std::format("read line {}: invalid version. Only 1.0 is supported", lIdx));
			}
			else {
				auto a = split(line, str::space);
				switch (line[0]) {
				case 'c': { // comment ...
					if (a[0] != str::comment)
						throw std::runtime_error(std::format("read line {}: invalid", lIdx));
					comments_.push_back(line.substr(a[0].size()));
				} break;
				case 'e': { // element name size
					if (a[0] != str::element)
						throw std::runtime_error(std::format("read line {}: invalid", lIdx));
					lastElement = &this->operator()(a[1], std::stoi(a[2]));
				} break;
				case 'p': { // property
					if (a[0] != str::property)
						throw std::runtime_error(std::format("read line {}: invalid", lIdx));
					if (a[1] == str::list) { // property list type type name
						lastElement->operator()(a[4], typeidFromStr(a[3], true));
						auto listTid = typeidFromStr(a[2], false);
						if(listTid != typeid(std::uint8_t) && listTid != typeid(std::uint16_t) && listTid != typeid(std::uint32_t))
							throw std::runtime_error(std::format("read line {}: invalid property list index type", lIdx));
						lastElement->operator()(a[4]).listIndexSize_ =
							listTid == typeid(std::uint8_t) ? 1 : listTid == typeid(std::uint16_t) ? 2 : 4;
					}
					else { // property type name
						lastElement->operator()(a[2], typeidFromStr(a[1], false));
					}
				} break;
				default: throw std::runtime_error(std::format("read line {}: invalid", lIdx));
				}
			}
			lIdx++;
		}
#ifdef USE_CRLF_LFCR_HEADER_HACK
		auto crlfa = *reinterpret_cast<std::array<std::uint16_t, 2>*>(&crlfCounter);
		// Problem: per definition "end_header\r" is the end of the header, but many implementations also
		// use "end_header\n" or "end_header\r\n" (or even "end_header\n\r"... sure). Due to the previous
		// getline(in, line), one of these was removed, but in the case of crlf/lfcr, one will remain.
		// if the first byte is now '\r', should we remove it? (it might be part of the data)
		// if the first byte is now '\n', should we remove it? (it might be part of the data)
		float ratio = static_cast<float>(crlfa[0]) / (crlfa[0] + crlfa[1]);
		if (std::abs(ratio - 0.5) < 0.1) { // probably crlf or lfcr was used... maybe....
			if (lastDecodedChar == str::cr && in.peek() == str::lf) in.get();
			if (lastDecodedChar == str::lf && in.peek() == str::cr) in.get();
		}
#endif
		for (std::size_t i = 0; i < order_.size(); i++) {
			auto& e = elements_[order_[i]];
			if(fmt == format::ascii)
				e.read<format::ascii>(in);
			else {
				if (endian == std::endian::little)
					e.read<format::binary, std::endian::little>(in);
				else
					e.read<format::binary, std::endian::big>(in);
			}
		}
	}

	template<format ff, std::endian ee>
	void root::write(std::string const& path) const {
		std::ofstream out(path, std::ios::binary | std::ios::trunc | std::ios::out);
		write<ff, ee>(out);
	}

	std::string root::str() const {
		std::ostringstream out;
		write<format::ascii>(out);
		return out.str();
	}

	template<format ff, std::endian ee>
	void root::write(std::ostream& out) const {
		out << str::ply << linesep_;
		for (auto const& c : comments_)
			if (c.find_first_of(str::invalidSymbols) == std::string::npos) // filter all the illegal stuff
				out << str::comment << " " << c << linesep_;
		out << str::format << " " << formatName<ff, ee>() << " " << str::version << linesep_;
		auto ne = order_.size();
		for (std::size_t i = 0; i < ne; i++)
		{
			auto const& e = elements_.at(order_[i]);
			out << str::element << " " << e.name() << " " << e.size() << linesep_;
			for (auto const& pname : e.order_) {
				auto const & p = e.properties_.at(pname);
				auto const & io = *ios_.at(p.tid_).get();
				auto const & info = *info_.at(p.tid_).get();
				out << str::property << " ";
				if (info.isList()) out << str::list << " " << listIndexTypeName(info.listIndexTypeSize(p.any_)) << " ";
				out << io.names()[0] << " " << p.name() << linesep_;
			}
		}
		out << str::end_header << linesep_;
		for (std::size_t i = 0; i < ne; i++) {
			elements_.at(order_[i]).write<ff, ee>(out);
			if constexpr(ff == format::ascii)
				if (i < ne - 1) out << linesep_;
		}
	}

	namespace {

		// can be replaced by std::byteswap(x) if everyone _HAS_CXX23
		template<std::integral T> inline constexpr T byteswap(T x) {
			if constexpr (sizeof(T) == 1)
				return x;
			else if constexpr (sizeof(T) == 2)
				return static_cast<T>((x << 8) | (x >> 8));
			else if constexpr (sizeof(T) == 4)
				return static_cast<T>((x << 24) | ((x << 8) & 0x00FF0000) | ((x >> 8) & 0x0000FF00) | (x >> 24));
			else if constexpr (sizeof(T) == 8)
				return static_cast<T>((x << 56) | ((x << 40) & 0x00FF000000000000) | ((x << 24) & 0x0000FF0000000000) | ((x << 8) & 0x000000FF00000000) | ((x >> 8) & 0x00000000FF000000) | ((x >> 24) & 0x0000000000FF0000) | ((x >> 40) & 0x000000000000FF00) | (x >> 56));
		}
		template<typename T> inline constexpr void write(std::ostream& out, T x, bool swapEndian) {
			if (swapEndian) {
				auto b = byteswap(*reinterpret_cast<const uintX<sizeof(T)>*>(&x));
				out.write(reinterpret_cast<const char*>(&b), sizeof(T));
			}
			else {
				out.write(reinterpret_cast<const char*>(&x), sizeof(T));
			}
		}
		template<typename T> inline constexpr void read(std::istream& in, T& x, bool swapEndian) {
			if (swapEndian) {
				uintX<sizeof(T)> b;
				in.read(reinterpret_cast<char*>(&b), sizeof(T));
				b = byteswap(b);
				x = *reinterpret_cast<T*>(&b);
			}
			else {
				in.read(reinterpret_cast<char*>(&x), sizeof(T));
			}
		}
		template<typename T, bool isList>
		struct io_x : public io {
			void ascI(std::istream& in, void* ptr, std::size_t i) const override {
				if constexpr (isList) {
					auto& vv = *reinterpret_cast<vec<vec<T>>*>(ptr);
					if (!vv.size()) return;
					auto& v = vv[i];
					std::size_t size = 0;
					in >> size;
					v.resize(size);
					if constexpr(sizeof(T) == 1) {
						for (auto& x : v) {
							in >> size;
							x = static_cast<T>(size);
						}
					}
					else 
						for (auto& x : v) in >> x;
				}
				else if constexpr (!isList) {
					auto& v = *reinterpret_cast<vec<T>*>(ptr);
					in >> v[i];
				}
			}
			void ascO(std::ostream& out, const void* ptr, std::size_t i) const override {
				if constexpr (isList) {
					auto& vv = *reinterpret_cast<const vec<vec<T>>*>(ptr);
					if (!vv.size()) return;
					auto& v = vv[i];
					out << v.size();
					if constexpr (std::is_same<T, float>::value)
						for (auto& x : v) out << " " << std::format(str::f32fmt, x);
					else if constexpr (std::is_same<T, double>::value)
						for (auto& x : v) out << " " << std::format(str::f64fmt, x);
					else if constexpr (sizeof(T) == 1)
						for (auto& x : v) out << " " << static_cast<int>(x);
					else
						for (auto& x : v) out << " " << x;
				}
				else if constexpr (!isList) {
					auto& v = *reinterpret_cast<const vec<T>*>(ptr);
					if constexpr (std::is_same<T, float>::value)
						out << std::format(str::f32fmt, v[i]);
					else if constexpr (std::is_same<T, double>::value)
						out << std::format(str::f64fmt, v[i]);
					else if constexpr (sizeof(T) == 1)
						out << static_cast<int>(v[i]);
					else
						out << v[i];
				}
			}
			void binI(std::istream& in, void* ptr, std::size_t i, std::uint8_t listIndexTypeSize, bool swapEndian) const override {
				if constexpr (isList) {
					auto& vv = *reinterpret_cast<vec<vec<T>>*>(ptr);
					if (!vv.size()) return;
					auto& v = vv[i];
					switch (listIndexTypeSize)
					{
						case 1: { uintX<1> x; read(in, x, swapEndian); v.resize(static_cast<std::size_t>(x)); } break;
						case 2: { uintX<2> x; read(in, x, swapEndian); v.resize(static_cast<std::size_t>(x)); } break;
						case 4: { uintX<4> x; read(in, x, swapEndian); v.resize(static_cast<std::size_t>(x)); } break;
						default: throw std::runtime_error(std::format("Invalid list index type size: {}", listIndexTypeSize));
					}
					for (auto& x : v) read(in, x, swapEndian);
				}
				else if constexpr (!isList) {
					auto& v = *reinterpret_cast<vec<T>*>(ptr);
					read(in, v[i], swapEndian);
				}
			}
			void binO(std::ostream& out, const void* ptr, std::size_t i, std::uint8_t listIndexTypeSize, bool swapEndian) const override {
				if constexpr (isList) {
					auto& vv = *reinterpret_cast<const vec<vec<T>>*>(ptr);
					if (!vv.size()) return;
					auto& v = vv[i];
					switch (listIndexTypeSize)
					{
						case 1: { write(out, static_cast<uintX<1>>(v.size()), swapEndian); } break;
						case 2: { write(out, static_cast<uintX<2>>(v.size()), swapEndian); } break;
						case 4: { write(out, static_cast<uintX<4>>(v.size()), swapEndian); } break;
						default: throw std::runtime_error(std::format("Invalid list index type size: {}", listIndexTypeSize));
					}
					for (auto& x : v) write(out, x, swapEndian);
				}
				else if constexpr (!isList) {
					auto& v = *reinterpret_cast<const vec<T>*>(ptr);
					write(out, v[i], swapEndian);
				}
			}
			vec<std::string_view> names() const override {
				if      constexpr (std::is_same<T, std::int8_t  >::value) return { str::t_char, str::t_int8 };
				else if constexpr (std::is_same<T, std::uint8_t >::value) return { str::t_uchar, str::t_uint8 };
				else if constexpr (std::is_same<T, std::int16_t >::value) return { str::t_short, str::t_int16 };
				else if constexpr (std::is_same<T, std::uint16_t>::value) return { str::t_ushort, str::t_uint16 };
				else if constexpr (std::is_same<T, std::int32_t >::value) return { str::t_int, str::t_int32 };
				else if constexpr (std::is_same<T, std::uint32_t>::value) return { str::t_uint, str::t_uint32 };
				else if constexpr (std::is_same<T, float        >::value) return { str::t_float, str::t_float32 };
				else if constexpr (std::is_same<T, double       >::value) return { str::t_double, str::t_float64 };
				else return {};
			}
		};
	}

	root::root() { // load default (ply 1.0) datatypes on construction.
		registerType<float, io_x>();
		registerType<double, io_x>();
		registerType<std::int8_t, io_x>();
		registerType<std::uint8_t, io_x>();
		registerType<std::int16_t, io_x>();
		registerType<std::uint16_t, io_x>();
		registerType<std::int32_t, io_x>();
		registerType<std::uint32_t, io_x>();
	}

}
