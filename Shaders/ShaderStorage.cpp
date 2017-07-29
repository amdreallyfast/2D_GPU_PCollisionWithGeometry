#include "ShaderStorage.h"

// for GL typedefs
#include "ThirdParty/glload/include/glload/gl_4_4.h"

// for making program from shader collection
#include <string>
#include <fstream>
#include <sstream>

#include <iostream>
using std::cout;
using std::endl;


/*------------------------------------------------------------------------------------------------
Description:
    My compute shaders are requiring a number of dependencies, and these are getting unwieldy.  
    I've already been using my own "REQUIRES" syntax at the top of my shaders just to keep track 
    of what files it needs, and I decided to add a delimiter in the form of a '|' character to 
    these lines.  This function takes advantage of those to try to find any files that a shader 
    depends on.  This should clean up my shader assembly.

    Note: This relies of std::getline(...)'s ability to read the stream up to and including the 
    end of file looking for the delmiter.  It's still called "get line".  I don't know why, but 
    it works :).
    http://www.cplusplus.com/reference/string/string/getline/
Parameters: 
    newFileStream   The file should be opened and ready to read.
    putDataHere     Self-explanatory.  
                    Chosen instead of a return file to avoid copying very large strings.
Returns:    None
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
void RecursivelyAddFileDependencies(std::ifstream &newFileStream, std::string &putDataHere)
{
    // dump the whole file into a stream in memory
    std::stringstream fileContentStream;
    fileContentStream << newFileStream.rdbuf();
    std::string line;
    std::string req("// REQUIRES ");
    while (!fileContentStream.eof())
    {
        std::getline(fileContentStream, line);
        if (line.find(req) == std::string::npos)
        {
            putDataHere += line + "\n";
        }
        else
        {
            // have a dependency file, replace the "REQUIRES" line with it
            line = line.substr(req.length());
            std::ifstream dependencyFileStream(line);
            if (!dependencyFileStream.is_open())
            {
                cout << __FILE__ << ", " << __FUNCTION__ << " cannot open dependency file " << line << endl;
            }
            else
            {
                std::string s;
                RecursivelyAddFileDependencies(dependencyFileStream, s);
                putDataHere += s;
            }
        }
    }
}

/*------------------------------------------------------------------------------------------------
Description:
    It's a getter for a singleton...and...that's it.
Parameters: None
Returns:    None
Creator:    John Cox (8-20-2016)
------------------------------------------------------------------------------------------------*/
ShaderStorage &ShaderStorage::GetInstance()
{
    static ShaderStorage instance;
    return instance;
}

/*------------------------------------------------------------------------------------------------
Description:
    Ensures that the object starts object with initialized values.
Parameters: None
Returns:    None
Creator:    John Cox (7-14-2016)
------------------------------------------------------------------------------------------------*/
ShaderStorage::ShaderStorage()
{
    //??anything??
}

/*------------------------------------------------------------------------------------------------
Description:
    Makes sure that there are no shader binaries or compiled programs associated with this 
    storage object that are still active.  It does not delete the maps, but just calls 
    glDeleteShader(...) and glDeleteProgram(...) as necessary to properly clean up.
Parameters: None
Returns:    None
Creator:    John Cox (7-16-2016)
------------------------------------------------------------------------------------------------*/
ShaderStorage::~ShaderStorage()
{
    for (_BINARY_MAP::iterator itr = _shaderBinaries.begin(); itr != _shaderBinaries.end(); itr++)
    {
        for (size_t shaderIndex = 0; shaderIndex < itr->second.size(); shaderIndex++)
        {
            GLuint shaderId = itr->second[shaderIndex];
            glDeleteShader(shaderId);
        }
    }

    for (_PROGRAM_MAP::iterator itr = _compiledPrograms.begin(); itr != _compiledPrograms.end(); itr++)
    {
        glDeleteProgram(itr->second);
    }

    // let the maps destruct themselves
}

/*------------------------------------------------------------------------------------------------
Description:
    Creates an empty section in the _shaderBinaries collection under the provided key that is 
    ready to accept compiled binaries from AddAndCompileShaderFile(...).  Must be called prior 
    to calling that function for the same program key.

    Prints errors to stderr.
Parameters: 
    programKey  The name that will be used to refer to this shader for the rest of the program.
                If it already exists, it prints a message to stderr and immediately returns.
Returns:    None
Creator:    John Cox, 3/11/2017
------------------------------------------------------------------------------------------------*/
void ShaderStorage::NewShader(const std::string &programKey)
{
    if (_shaderBinaries.find(programKey) != _shaderBinaries.end())
    {
        fprintf(stderr, "unlinked binary already exists for program key '%s'\n",
            programKey.c_str());
    }
    else
    {
        _shaderBinaries.insert({ programKey, _BINARY_MAP::value_type::second_type() });
    }
}

/*------------------------------------------------------------------------------------------------
Description:
    Deletes shader file contents that are still being added to, compiled shader binaries, and
    the linked program, if any of them exist.

    This is a manual cleanup method.  If not called, everything will be cleaned up properly in 
    the destructor.
    
    Failure to find something for the program key does not print an error or abort, but finding 
    and deleting an entry will print a note.
Parameters:
    programKey  Delete the compiled shader program associated with this string.
Returns:    None
Creator:    John Cox, 3/11/2107
------------------------------------------------------------------------------------------------*/
void ShaderStorage::DeleteShader(const std::string &programKey)
{
    _BINARY_MAP::iterator binariesItr = _shaderBinaries.find(programKey);
    if (binariesItr != _shaderBinaries.end())
    {
        for (size_t shaderIndex = 0; shaderIndex < binariesItr->second.size(); shaderIndex++)
        {
            GLuint shaderId = binariesItr->second[shaderIndex];
            glDeleteShader(shaderId);
        }

        _shaderBinaries.erase(binariesItr);
        fprintf(stdout, "Deleting compiled binaries for program key '%s'\n", programKey.c_str());
    }

    _PROGRAM_MAP::iterator programItr = _compiledPrograms.find(programKey);
    if (programItr != _compiledPrograms.end())
    {
        glDeleteProgram(programItr->second);
        _compiledPrograms.erase(programItr);
        fprintf(stdout, "Deleting linked program from program key '%s'\n", programKey.c_str());
    }
}

/*------------------------------------------------------------------------------------------------
Description:
    Reads the file contents and attempts to compile a shader binary.

    Prints its own errors to stderr.  The APIENTRY debug function doesn't report shader compile
    errors.
Parameters:
    programKey  Must have already been created by NewShader(...).
    filePath    Can be relative to program or an absolute path.
    shaderType  ??
Returns:    None
Creator:    John Cox, 3/11/2107
------------------------------------------------------------------------------------------------*/
void ShaderStorage::AddAndCompileShaderFile(const std::string &programKey, const std::string &filePath, const GLenum shaderType)
{
    if (_shaderBinaries.find(programKey) == _shaderBinaries.end())
    {
        fprintf(stderr, "Could not add shader file '%s'.  No program key '%s'\n", 
            filePath.c_str(), programKey.c_str());
        return;
    }

    std::ifstream shaderFile(filePath);
    if (!shaderFile.is_open())
    {
        fprintf(stderr, "Cannot open shader file '%s'\n", filePath.c_str());
        return;
    }

    std::string fileContents;
    RecursivelyAddFileDependencies(shaderFile, fileContents);

//#define DEBUG
#ifdef DEBUG
    // useful because the OpenGL shader compilation spits out errors as if the shader were one 
    // file, and shaders with dependency files dumped into it are not
    cout << endl << endl << "----------------------------------" << endl << endl;
    std::stringstream ss(fileContents);
    std::string line;
    int lineCounter = 0;
    while (!ss.eof())
    {
        std::getline(ss, line);
        char lineNumberStr[6];
        snprintf(lineNumberStr, 6, "%-5d", lineCounter);
        cout << lineNumberStr << line << endl;
        lineCounter++;
    }
#endif // DEBUG


    if (fileContents.empty())
    {
        fprintf(stderr, "Shader file '%s' is empty\n", filePath.c_str());
        return;
    }

    GLuint shaderId = CompileShader(fileContents, shaderType);
    if (shaderId == 0)
    {
        fprintf(stderr, "Problem compiling shader for program key '%s'\n", programKey.c_str());
        return;
    }
    
    // compilation went fine
    // Note: The [] notation on a map will automatically add the key-value pair if it 
    // doesn't exist, but the iterator search at function start will protect against errant 
    // inserations, so this notation is ok.
    _shaderBinaries[programKey].push_back(shaderId);
}

/*------------------------------------------------------------------------------------------------
Description:
    Links the collection of binaries under the specified key into a compiled program, deletes 
    the shader binaries (no longer needed), adds the compiled program to the internal collection 
    of compiled shader programs, and returns the shader ID.

    Prints its own errors to stderr.  The APIENTRY debug function doesn't report shader link
    errors.
Parameters:
    programKey  Must have already been created by NewShader(...).
Returns:
    The ID of the resultant program, or 0 if no program was compiled (hopefully 0 is still an
    invalid value in the future.
Creator:    John Cox (7-14-2016)
------------------------------------------------------------------------------------------------*/
GLuint ShaderStorage::LinkShader(const std::string &programKey)
{
    _BINARY_MAP::iterator itr = _shaderBinaries.find(programKey);
    if (itr == _shaderBinaries.end() ||
        itr->second.empty())
    {
        fprintf(stderr, "No shader binaries under the key '%s'\n", programKey.c_str());
        return 0;
    }

    GLuint programId = glCreateProgram();

    // the collection of shader binaries is the second item in the map's string-binary pairs
    // Note: In many cases, there will only be two items: a vertex and fragment shader.
    for (size_t shaderIndex = 0; shaderIndex < itr->second.size(); shaderIndex++)
    {
        glAttachShader(programId, itr->second[shaderIndex]);
    }
    glLinkProgram(programId);

    // the program contains linked versions of the shaders, so the compiled binary objects 
    // are no longer necessary
    // Note: Shader objects need to be un-linked before they can be deleted.  This is ok 
    // because the program safely contains the shaders in binary form.
    for (size_t shaderIndex = 0; shaderIndex < itr->second.size(); shaderIndex++)
    {
        GLuint shaderId = itr->second[shaderIndex];
        glDetachShader(programId, shaderId);
        glDeleteShader(shaderId);
    }

    // check if the program was built ok
    // Note: Perform this check after the shader objects were already cleaned up.  It makes
    // the program cleanup easier.
    GLint isLinked = 0;
    glGetProgramiv(programId, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLchar errLog[128];
        GLsizei *logLen = 0;
        glGetProgramInfoLog(programId, 128, logLen, errLog);
        fprintf(stderr, "Program '%s' didn't link: '%s'\n", programKey.c_str(), errLog);
        glDeleteProgram(programId);
        return 0;
    }

    _compiledPrograms[programKey] = programId;
    return programId;
}

/*------------------------------------------------------------------------------------------------
Description:
    A getter for a compiled program.  
    
    Prints errors to stderr.
Parameters:
    programKey  The string key that was used for adding shader files and linking the binaries.
Returns:
    The program ID for the requested shader, or 0 if requested shader not found.
Creator:    John Cox (7-14-2016)
------------------------------------------------------------------------------------------------*/
GLuint ShaderStorage::GetShaderProgram(const std::string &programKey) const
{
    _PROGRAM_MAP::const_iterator compiledItr = _compiledPrograms.find(programKey);
    if (compiledItr == _compiledPrograms.end())
    {
        _BINARY_MAP::const_iterator binaryItr = _shaderBinaries.find(programKey);
        if (binaryItr == _shaderBinaries.end())
        {
            fprintf(stderr, "No shader program under the key '%s'\n", programKey.c_str());
        }
        else
        {
            fprintf(stderr, "No shader program for key '%s', but there are unlinked binaries\n", programKey.c_str());
        }
        
        return 0;
    }

    return compiledItr->second;
}

/*------------------------------------------------------------------------------------------------
Description:
    A getter for a uniform location within a compiled program.  

    Prints errors to stderr.
Parameters:
    programKey  The string key that was used for adding shader files and linking the binaries.
    uniformName The string that spells out verbatim a uniform name within the requested shader.
Returns:
    A uniform location, or -1 if (1) the requested program doesn't exist or (2) the uniform 
    could not be found.
Creator:    John Cox (7-14-2016)
------------------------------------------------------------------------------------------------*/
GLint ShaderStorage::GetUniformLocation(const std::string &programKey,
    const std::string &uniformName) const
{
    _PROGRAM_MAP::const_iterator itr = _compiledPrograms.find(programKey);
    if (itr == _compiledPrograms.end())
    {
        fprintf(stderr, "No shader program under the key '%s'\n", programKey.c_str());
        return -1;
    }

    GLint programId = itr->second;

    GLint uniformLocation = glGetUniformLocation(programId, uniformName.c_str());
    if (uniformLocation < 0)
    {
        fprintf(stderr, "No uniform '%s' in shader program '%s'\n", uniformName.c_str(), programKey.c_str());
    }
    
    return uniformLocation;
}

/*------------------------------------------------------------------------------------------------
Description:
    A getter for a uniform location within a compiled program.  

    Prints errors to stderr.
Parameters:
    programId   A value somehow retrieved from ShaderStorage::GetShaderProgram(...).
    uniformName The string that spells out verbatim a uniform name within the requested shader.
Returns:
    A uniform location, or -1 if (1) the requested program doesn't exist or (2) the uniform 
    could not be found.
Creator:    John Cox, 7/2017
------------------------------------------------------------------------------------------------*/
GLint ShaderStorage::GetUniformLocation(GLuint programId, const std::string &uniformName) const
{
    GLint uniformLocation = glGetUniformLocation(programId, uniformName.c_str());
    if (uniformLocation < 0)
    {
        // uncomment if you want to be told when a uniform is not being used
        // Note: If the program is not 0, then it compiled and linked just fine, so if the 
        // uniform can't be found, then the program creation found that the uniform was not 
        // being used and therefore optimized it out.  This is not a problem and doesn't need to 
        // be reported unless the user wants to find unused uniforms.
        // Also Note: According to the OpenGL spec, if the location is -1, "the data passed to 
        // glUniform*(...) will be silently ignored and the specified uniform variable will not 
        // be changed."
        // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/glUniform.xhtml
        fprintf(stderr, "No uniform '%s' in shader program '%u'\n", uniformName.c_str(), programId);
    }

    return uniformLocation;
}

/*------------------------------------------------------------------------------------------------
Description:
    A getter for an attribute location within a compiled program.  
    
    Prints errors to stderr.
Parameters:
    programKey      The string key that was used for adding shader files and linking the 
                    binaries.
    attributeName   The string that spells out verbatim a uniform name within the requested 
                    shader.
Returns:
    An attribute location, or -1 if (1) the requested program doesn't exist or (2) the uniform 
    could not be found.
Creator:    John Cox (7-14-2016)
------------------------------------------------------------------------------------------------*/
GLint ShaderStorage::GetAttributeLocation(const std::string &programKey,
    const std::string &attributeName) const
{
    _PROGRAM_MAP::const_iterator itr = _compiledPrograms.find(programKey);
    if (itr == _compiledPrograms.end())
    {
        fprintf(stderr, "No shader program under the key '%s'\n", programKey.c_str());
        return -1;
    }

    GLint programId = itr->second;
    GLint attributeLocation = glGetAttribLocation(programId, attributeName.c_str());
    if (attributeLocation < 0)
    {
        fprintf(stderr, "No attribute '%s' in shader program '%s'\n", attributeName.c_str(),
            programKey.c_str());
        // already 0, so don't bother explicitly returning 0
    }

    return attributeLocation;
}

/*------------------------------------------------------------------------------------------------
Description:
    Creates an empty string under the provided key that is ready to accept file contents from 
    AddPartialShaderFile(...).  Must be called prior to calling that function for the same program 
    key.

    Prints errors to stderr.
Parameters: 
    programKey  The name that will be used to refer to this shader for the rest of the program.
                If it already exists, it prints a message to stderr and immediately returns.
Returns:    None
Creator:    John Cox, 3/2017
------------------------------------------------------------------------------------------------*/
GLuint ShaderStorage::CompileShader(const std::string &shaderAsString, const GLenum shaderType)
{
    if (shaderAsString.empty())
    {
        fprintf(stderr, "Shader is empty.\n");
        return 0;
    }

    // OpenGL takes pointers to file contents and pointers to file content lengths, so use arrays
    const GLchar *bytes[] = { shaderAsString.c_str() };
    const GLint strLengths[] = { (int)shaderAsString.length() };

    GLuint shaderId = glCreateShader(shaderType);

    // add the file contents to the shader
    // Note: An additional step is required for shader compilation.
    glShaderSource(shaderId, 1, bytes, strLengths);
    glCompileShader(shaderId);

    // ??necessary or will the APIENTRY debug function handle this??
    GLint isCompiled = 0;
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLchar errLog[128];
        GLsizei *logLen = 0;
        glGetShaderInfoLog(shaderId, 128, logLen, errLog);
        fprintf(stderr, "shader failed: '%s'\n", errLog);
        glDeleteShader(shaderId);

        return 0;
    }
    else
    {
        // compilation went fine
        return shaderId;
    }
}

