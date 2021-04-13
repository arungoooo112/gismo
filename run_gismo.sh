# Run gismo files
echo "P = 3, R = 1"
#echo ./bin/biharmonic_argyris_example -g -1 -l 4
#./bin/biharmonic_argyris_example -g -1 -l 4

for pp in 3 4 5
do
    for i in 1100 1020 1021 1000 1010 1011
    do
    echo ./bin/biharmonic_argyris_example -g $i -p $pp -r 1 -l 7 --csv
    ./bin/biharmonic_argyris_example -g $i -p $pp -r 1 -l 7 --csv
    done
done

echo Finished!