# rcom-lab1 - serial port

## Description
Simple connection protocol to send a file, in this case a `.gif`, through a serial port, physical or virtual, with accurate error detection and reliable recovery.

## Requirements
- [gcc](https://gcc.gnu.org/)
- [socat](https://linux.die.net/man/1/socat)

## How to run

If you want to get more details about the code after reading, follow [here](code/README.txt).

### Virtual
To virtually run the program, go to the `code/` folder and compile the programs with:
```bash
make clean
make all
```
After that, open a virtual socat cable connection using:
```bash
sudo make run_cable
```
Open two separate terminals on the same folder and run the following commands on each one, respectively:
```bash
make run_rx
```
```bash
make run_tx
```
![running the program](https://github.com/joaonevesf/rcom-lab1/assets/93282636/239d3ac7-d5a8-4d27-afc4-fb82af384a57)

To check if the file sent successfully and didn't have any losses, run the following command:

```
make check_files
```

![checking the files](https://github.com/joaonevesf/rcom-lab1/assets/93282636/2857c4d8-7f16-4ea1-be5f-63f0698ce05f)

Virtually, you can also test the realiability of the program using the cable flags below:

```bash
--- on           : connect the cable and data is exchanged (default state)
--- off          : disconnect the cable disabling data to be exchanged
--- noise        : add fixed noise to the cable
--- end          : terminate the program
```

### Physical
To run the program physically, you need two computers connected by a serial port.

After that, compile the programs like before and run the respective commands on each computer.

On first computer:
```bash
make clean
make all
make run_rx
```
On second computer:
```bash
make clean
make all
make run_tx
```

To cut the connection, simply disconnect the serial port temporarily.

To introduce noise, you could add/remove eletricity from the cables. You could use a metal object on the cables to get this effect, for example.

## Authors
- João Fernandes [joaonevesf](https://github.com/joaonevesf)
- Pedro Oliveira [pfpo](https://github.com/pfpo)
