cmake_minimum_required(VERSION 3.0.0)
project(qwt_examples VERSION 0.1.0)



#make sure MOC compiler is called in parallel
#set(CMAKE_AUTOGEN_PARALLEL AUTO)

# Find includes in corresponding build directories
#set(CMAKE_INCLUDE_CURRENT_DIR ON)

# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)

# Instruct CMake to create code from Qt resource files (*.qrc)
#set(CMAKE_AUTORCC ON)

# UI ==> H generator from QT   # Instruct CMake to create code from Qt designer ui files
#set(CMAKE_AUTOUIC ON)

# # Find the QtWidgets library
find_package(Qt5Widgets CONFIG REQUIRED)
# find_package(Qt5OpenGL CONFIG REQUIRED)
# find_package(Qt5Charts CONFIG REQUIRED)
# find_package(Qt5PrintSupport CONFIG REQUIRED)
# find_package(Qt5Concurrent CONFIG REQUIRED)
# find_package(Qt5OpenGL CONFIG REQUIRED)

MACRO(ADD_EXAMPLE   example_name)
    set(SRC_FILES "")
    aux_source_directory(./${example_name} SRC_FILES)
    add_executable(${example_name} ${SRC_FILES})
    target_link_libraries(${example_name} qwt)
ENDMACRO()

ADD_EXAMPLE(animation)
ADD_EXAMPLE(barchart)
ADD_EXAMPLE(bode)
ADD_EXAMPLE(controls)
ADD_EXAMPLE(cpuplot)
ADD_EXAMPLE(curvedemo)
ADD_EXAMPLE(dials)
ADD_EXAMPLE(distrowatch)
ADD_EXAMPLE(friedberg)
ADD_EXAMPLE(itemeditor)
ADD_EXAMPLE(legends)
ADD_EXAMPLE(oscilloscope)
ADD_EXAMPLE(polardemo)
ADD_EXAMPLE(polarspectrogram)
ADD_EXAMPLE(radio)
ADD_EXAMPLE(rasterview)
ADD_EXAMPLE(realtime)
ADD_EXAMPLE(refreshtest)
ADD_EXAMPLE(scatterplot)
ADD_EXAMPLE(simpleplot)
ADD_EXAMPLE(sinusplot)
ADD_EXAMPLE(spectrogram)
ADD_EXAMPLE(splineeditor)
ADD_EXAMPLE(stockchart)
ADD_EXAMPLE(sysinfo)
ADD_EXAMPLE(tvplot)

