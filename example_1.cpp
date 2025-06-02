

#include <okayply.h>
#include <iostream>

#define Example 2

#if Example == 1

int main()
{
	try {
		okayply::root ply;
		{
			// Add element "vertex" with size 14
			auto& vertices = ply("vertex", 14);

			// Add element "face" with size 6
			auto& faces = ply("face", 6);

			// Initialize vertex.x as float (typeid is sufficient, no templates needed)
			auto& px = vertices("x", typeid(float));

			// Access the data (this needs a template so x can have the right type)
			auto x = px.get<float>();

			// Late inizialize vertex.y as float and access the data in one step
			auto y = vertices("y").get<float>();

			// Also works in combination with access from root (only if "vertex" is already initialized with a size!)
			auto z = ply("vertex")("z").get<float>();

			// For all the others
			auto nx = vertices("nx").get<float>();
			auto ny = vertices("ny").get<float>();
			auto nz = vertices("nz").get<float>();
			auto s = vertices("s").get<float>();
			auto t = vertices("t").get<float>();

			// C&P from the wikipedia example
			x[0] = 1; y[0] = 1; z[0] = 1, nx[0] = 0.5773503f; ny[0] = 0.5773503f, nz[0] = 0.5773503f; s[0] = 0.625f; t[0] = 0.50f;
			x[1] = -1; y[1] = 1; z[1] = 1, nx[1] = -0.5773503f; ny[1] = 0.5773503f, nz[1] = 0.5773503f; s[1] = 0.875f; t[1] = 0.50f;
			x[2] = -1; y[2] = -1; z[2] = 1, nx[2] = -0.5773503f; ny[2] = -0.5773503f, nz[2] = 0.5773503f; s[2] = 0.625f; t[2] = 0.75f;
			x[3] = 1; y[3] = -1; z[3] = 1, nx[3] = 0.5773503f; ny[3] = -0.5773503f, nz[3] = 0.5773503f; s[3] = 0.625f; t[3] = 0.75f;
			x[4] = 1; y[4] = -1; z[4] = -1, nx[4] = 0.5773503f; ny[4] = -0.5773503f, nz[4] = -0.5773503f; s[4] = 0.375f; t[4] = 0.75f;
			x[5] = -1; y[5] = -1; z[5] = 1, nx[5] = -0.5773503f; ny[5] = -0.5773503f, nz[5] = 0.5773503f; s[5] = 0.625f; t[5] = 1.00f;
			x[6] = -1; y[6] = -1; z[6] = -1, nx[6] = -0.5773503f; ny[6] = -0.5773503f, nz[6] = -0.5773503f; s[6] = 0.375f; t[6] = 1.00f;
			x[7] = -1; y[7] = -1; z[7] = -1, nx[7] = -0.5773503f; ny[7] = -0.5773503f, nz[7] = -0.5773503f; s[7] = 0.375f; t[7] = 0.00f;
			x[8] = -1; y[8] = -1; z[8] = 1, nx[8] = -0.5773503f; ny[8] = -0.5773503f, nz[8] = 0.5773503f; s[8] = 0.625f; t[8] = 0.00f;
			x[9] = -1; y[9] = 1; z[9] = 1, nx[9] = -0.5773503f; ny[9] = 0.5773503f, nz[9] = 0.5773503f; s[9] = 0.625f; t[9] = 0.25f;
			x[10] = -1; y[10] = 1; z[10] = -1, nx[10] = -0.5773503f; ny[10] = 0.5773503f, nz[10] = -0.5773503f; s[10] = 0.375f; t[10] = 0.25f;
			x[11] = -1; y[11] = 1; z[11] = -1, nx[11] = -0.5773503f; ny[11] = 0.5773503f, nz[11] = -0.5773503f; s[11] = 0.125f; t[11] = 0.50f;
			x[12] = 1; y[12] = 1; z[12] = -1, nx[12] = 0.5773503f; ny[12] = 0.5773503f, nz[12] = -0.5773503f; s[12] = 0.375f; t[12] = 0.50f;
			x[13] = -1; y[13] = -1; z[13] = -1, nx[13] = -0.5773503f; ny[13] = -0.5773503f, nz[13] = -0.5773503f; s[13] = 0.125f; t[13] = 0.75f;
			auto vi = faces("vertex_indices").get<std::vector<int>>();
			vi[0].push_back(0); vi[0].push_back(1); vi[0].push_back(2); vi[0].push_back(3);
			vi[1].push_back(4); vi[1].push_back(3); vi[1].push_back(5); vi[1].push_back(6);
			vi[2].push_back(7); vi[2].push_back(8); vi[2].push_back(9); vi[2].push_back(10);
			vi[3].push_back(11); vi[3].push_back(12); vi[3].push_back(4); vi[3].push_back(13);
			vi[4].push_back(12); vi[4].push_back(0); vi[4].push_back(3); vi[4].push_back(4);
			vi[5].push_back(10); vi[5].push_back(9); vi[5].push_back(0); vi[5].push_back(12);
		}

		// we dont need s and t, delete them.
		ply("vertex").del("s");
		ply("vertex").del("t");

		// example ASCII output into console
		std::cout << "\n" << ply.str() << "\n";

		// Write some files
		ply.write<okayply::format::ascii>("outASC.ply");
		ply.write<okayply::format::binary, std::endian::little>("outBINLE.ply");
		ply.write<okayply::format::binary, std::endian::big>("outBINBE.ply");

		// Read some files
		{
			okayply::root ply2;
			ply2.read("outASC.ply");
			std::cout << "\n" << ply2.str() << "\n";
			ply2.read("outBINBE.ply");
			std::cout << "\n" << ply2.str() << "\n"; // will display ascii in the console because root.str() is always ascii
			ply2.read("outBINLE.ply");
			std::cout << "\n" << ply2.str() << "\n"; // will display ascii in the console because root.str() is always ascii

			// Access all elements & properties without bloat.
			for (auto& eref : ply2.elements()) {
				auto& e = eref.get();
				std::cout << "\nELEMENT\n";
				std::cout << "name: " << e.name() << ", ptr: " << &e << ", size: " << e.size() << "\n";
				std::cout << "PROPERTY\n";
				for (auto& pref : e.properties()) {
					auto& p = pref.get();
					if (p.type() == typeid(float)) {
						auto data = p.get<float>();
						std::cout << "name: " << p.name() << ", ptr: " << &p << ", type: " <<
							p.type().name() << ", list: " << (p.isList() ? "true" : "false") <<
							", listType: " << p.listType().name() << "\n";
						for (auto& x : data)
							std::cout << x << " ";
						std::cout << "\n";
					}
					else if (p.listType() == typeid(std::vector<int>))
					{
						auto data = p.get<std::vector<int>>();
						std::cout << "name: " << p.name() << ", ptr: " << &p << ", type: " <<
							p.type().name() << ", list: " << (p.isList() ? "true" : "false") <<
							", listType: " << p.listType().name() << "\n";
						for (auto& x : data) {
							for (auto& y : x)
								std::cout << y << " ";
							std::cout << "\n";
						}
						std::cout << "\n";
					}
				}
			}
		}

	}
	catch (std::exception& e) {
		std::cout << e.what();
	}
	return EXIT_SUCCESS;
}

#endif

#if Example == 2
int main() {
	try {
		// same as example 1 but compact
		okayply::root ply;
		{
			auto& vertices = ply("vertex", 14);
			auto& faces = ply("face", 6);
			auto const n = 0.5773503f;
			std::vector<float> xPositions = { 1, -1, -1,  1,  1, -1, -1, -1, -1, -1, -1, -1,  1, -1 };
			std::vector<float> xNormal = { n, -n, -n,  n,  n, -n, -n, -n, -n, -n, -n, -n,  n, -n };
			std::vector<float> yPositions = { 1,  1, -1, -1, -1, -1, -1, -1, -1,  1,  1,  1,  1, -1 };
			std::vector<float> yNormal = { n,  n, -n, -n, -n, -n, -n, -n, -n,  n,  n,  n,  n, -n };
			std::vector<float> zPositions = { 1,  1,  1,  1, -1,  1, -1, -1,  1,  1, -1, -1, -1, -1 };
			std::vector<float> zNormal = { n,  n,  n,  n, -n,  n, -n, -n,  n,  n, -n, -n, -n, -n };
			vertices("x").set(xPositions);
			vertices("y").set(yPositions);
			vertices("z").set(zPositions);
			vertices("nx").set(xNormal);
			vertices("ny").set(yNormal);
			vertices("nz").set(zNormal);
			std::vector<std::vector<int>> vertexIndices = {
				{ 0,  1, 2,  3},
				{ 4,  3, 5,  6},
				{ 7,  8, 9, 10},
				{11, 12, 4, 13},
				{12,  0, 3, 4},
				{10,  9, 0, 12}
			};
			faces("vertex_indices").set(vertexIndices);
		}
		std::cout << "\n" << ply.str() << "\n";
		ply.write("test.ply");
	}
	catch (std::exception& e) {
		std::cout << e.what();
	}
	return EXIT_SUCCESS;
}
#endif
