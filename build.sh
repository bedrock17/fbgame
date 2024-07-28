# 현재 디렉토리를 탐색해서 모든 cpp 파일을 컴파일하고 바이너리를 만든다.
# 바이너리는 output 디렉토리에 저장된다.

mkdir -p output
for file in $(find . -name "*.cpp"); do
    echo "Building $file"
    g++ $file -o output/$(basename $file .cpp)
done

cp *.bmp output/