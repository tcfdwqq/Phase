#include <boost/algorithm/string.hpp>

#include "Viewer.h"
#include "Exception.h"

Viewer::Viewer(const Solver &solver, const Input &input)
    :
      grid_(solver.grid()),
      caseName_(input.caseInput().get<std::string>("CaseName"))
{
    using namespace std;
    using namespace boost;

    outputFilename_ = input.outputPath + "/" + caseName_ + ".dat";

    string vectorFields = input.caseInput().get<string>("Viewer.vectorFields");
    string scalarFields = input.caseInput().get<string>("Viewer.scalarFields");

    vector<string> vectorFieldNames, scalarFieldNames;
    split(vectorFieldNames, vectorFields, is_any_of(", "), token_compress_on);
    split(scalarFieldNames, scalarFields, is_any_of(", "), token_compress_on);


}

Viewer::~Viewer()
{
    if(fout_.is_open())
        fout_.close();
}

void Viewer::openFile()
{
    if(!fout_.is_open())
        fout_.open(outputFilename_.c_str());
}