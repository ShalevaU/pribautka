//Shaleva U. gr 932424

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX 256
#define TOP_VALUE 65535
#define HALF (TOP_VALUE / 2 + 1)
#define FIRST_QTR (HALF / 2)
#define THIRD_QTR (FIRST_QTR * 3)

typedef struct
{
    unsigned char ch;
    unsigned int frequency;
    unsigned int low;
    unsigned int high;
} Symbol;

Symbol Symbols[MAX];
unsigned int Frequency_Table[MAX] = { 0 };
int Symbols_Count = 0;
unsigned int Total_Frequency = 0;
size_t Original_Size = 0;

unsigned char Out_Buffer = 0;
int Out_Bits = 0;

void Write_Bit(FILE* File_Output, int Bit);
void Flush_Bits(FILE* File_Output);
unsigned char In_Buffer = 0;
int In_Bits = 0;

int Read_Bit(FILE* File_Input);
void Build_Frequency_Table();
void Write_Frequency_Table(FILE* File_Output);
void Read_Frequency_Table(FILE* File_Input);
void Encode_File();
void Decode_File();
int check_Files_Equal(const char* File1, const char* File2);

void Write_Bit(FILE* File_Output, int Bit)
{
    Out_Buffer = (Out_Buffer << 1) | Bit;
    Out_Bits++;
    if (Out_Bits == 8)
    {
        fputc(Out_Buffer, File_Output);
        Out_Buffer = 0;
        Out_Bits = 0;
    }
}

void Flush_Bits(FILE* File_Output)
{
    if (Out_Bits > 0)
    {
        Out_Buffer <<= (8 - Out_Bits);
        fputc(Out_Buffer, File_Output);
    }
}

int Read_Bit(FILE* File_Input)
{
    if (In_Bits == 0)
    {
        int C = fgetc(File_Input);
        if (C == EOF) return 0;
        In_Buffer = (unsigned char)C;
        In_Bits = 8;
    }
    int Bit = (In_Buffer >> 7) & 1;
    In_Buffer <<= 1;
    In_Bits--;
    return Bit;
}

void Build_Frequency_Table()
{
    FILE* File_Input = fopen("text.txt", "rb");
    if (!File_Input)
    {
        printf_s("Error: Source file text.txt not found\n");
        exit(1);
    }
    int C;
    Original_Size = 0;
    while ((C = fgetc(File_Input)) != EOF)
    {
        Frequency_Table[C]++;
        Original_Size++;
    }
    fclose(File_Input);
    Symbols_Count = 0;
    for (int I = 0; I < MAX; I++)
    {
        if (Frequency_Table[I] > 0)
        {
            Symbols[Symbols_Count].ch = (unsigned char)I;
            Symbols[Symbols_Count].frequency = Frequency_Table[I];
            Symbols_Count++;
        }
    }
    unsigned int Cumulative = 0;
    for (int I = 0; I < Symbols_Count; I++)
    {
        Symbols[I].low = Cumulative;
        Cumulative += Symbols[I].frequency;
        Symbols[I].high = Cumulative;
    }
    Total_Frequency = Cumulative;
}

void Write_Frequency_Table(FILE* File_Output)
{
    fwrite(&Original_Size, sizeof(Original_Size), 1, File_Output);
    fwrite(&Symbols_Count, sizeof(Symbols_Count), 1, File_Output);
    for (int I = 0; I < Symbols_Count; I++)
    {
        fputc(Symbols[I].ch, File_Output);
        fwrite(&Symbols[I].frequency, sizeof(unsigned int), 1, File_Output);
    }
}

void Read_Frequency_Table(FILE* File_Input)
{
    fread(&Original_Size, sizeof(Original_Size), 1, File_Input);
    fread(&Symbols_Count, sizeof(Symbols_Count), 1, File_Input);
    Total_Frequency = 0;
    for (int I = 0; I < Symbols_Count; I++)
    {
        Symbols[I].ch = fgetc(File_Input);
        fread(&Symbols[I].frequency, sizeof(unsigned int), 1, File_Input);
        Symbols[I].low = Total_Frequency;
        Total_Frequency += Symbols[I].frequency;
        Symbols[I].high = Total_Frequency;
    }
}

void Encode_File()
{
    FILE* File_Input = fopen("text.txt", "rb");
    FILE* File_Output = fopen("encoded.txt", "wb");
    if (!File_Input || !File_Output) {
        printf_s("Error: Failed to open file\n");
        return;
    }
    Build_Frequency_Table();
    Write_Frequency_Table(File_Output);
    unsigned int low = 0, high = TOP_VALUE;
    int Bits_To_Follow = 0;
    int C;
    while ((C = fgetc(File_Input)) != EOF)
    {
        int Index = 0;
        while (Symbols[Index].ch != (unsigned char)C) Index++;
        unsigned int Range = high - low + 1;
        high = low + (Range * Symbols[Index].high) / Total_Frequency - 1;
        low = low + (Range * Symbols[Index].low) / Total_Frequency;
        while (1)
        {
            if (high < HALF)
            {
                Write_Bit(File_Output, 0);
                while (Bits_To_Follow > 0)
                {
                    Write_Bit(File_Output, 1);
                    Bits_To_Follow--;
                }
            }
            else if (low >= HALF)
            {
                Write_Bit(File_Output, 1);
                while (Bits_To_Follow > 0)
                {
                    Write_Bit(File_Output, 0);
                    Bits_To_Follow--;
                }
                low -= HALF;
                high -= HALF;
            }
            else if (low >= FIRST_QTR && high < THIRD_QTR)
            {
                Bits_To_Follow++;
                low -= FIRST_QTR;
                high -= FIRST_QTR;
            }
            else break;
            low <<= 1;
            high = (high << 1) | 1;
        }
    }
    Bits_To_Follow++;
    if (low < FIRST_QTR)
    {
        Write_Bit(File_Output, 0);
        while (Bits_To_Follow > 0)
        {
            Write_Bit(File_Output, 1);
            Bits_To_Follow--;
        }
    }
    else
    {
        Write_Bit(File_Output, 1);
        while (Bits_To_Follow > 0)
        {
            Write_Bit(File_Output, 0);
            Bits_To_Follow--;
        }
    }
    Flush_Bits(File_Output);
    fclose(File_Input);
    fclose(File_Output);
}

void Decode_File()
{
    FILE* File_Input = fopen("encoded.txt", "rb");
    FILE* File_Output = fopen("decoded.txt", "wb");
    if (!File_Input || !File_Output)
    {
        printf_s("Error: Failed to open file\n");
        return;
    }
    Read_Frequency_Table(File_Input);
    unsigned int low = 0, high = TOP_VALUE;
    unsigned int Value = 0;
    for (int I = 0; I < 16; I++)
    {
        Value = (Value << 1) | Read_Bit(File_Input);
    }
    for (size_t Count = 0; Count < Original_Size; Count++)
    {
        unsigned int Range = high - low + 1;
        unsigned int Freq = ((Value - low + 1) * Total_Frequency - 1) / Range;
        int Index = 0;
        while (Symbols[Index].high <= Freq) Index++;
        fputc(Symbols[Index].ch, File_Output);
        high = low + (Range * Symbols[Index].high) / Total_Frequency - 1;
        low = low + (Range * Symbols[Index].low) / Total_Frequency;
        while (1)
        {
            if (high < HALF) {}
            else if (low >= HALF)
            {
                Value -= HALF;
                low -= HALF;
                high -= HALF;
            }
            else if (low >= FIRST_QTR && high < THIRD_QTR)
            {
                Value -= FIRST_QTR;
                low -= FIRST_QTR;
                high -= FIRST_QTR;
            }
            else break;
            low <<= 1;
            high = (high << 1) | 1;
            Value = (Value << 1) | Read_Bit(File_Input);
        }
    }
    fclose(File_Input);
    fclose(File_Output);
}

int check_Files_Equal(const char* File1, const char* File2)
{
    FILE* F1 = fopen(File1, "rb");
    FILE* F2 = fopen(File2, "rb");
    if (!F1 || !F2)
    {
        if (F1) fclose(F1);
        if (F2) fclose(F2);
        return 0;
    }
    int A, B;
    while (1)
    {
        A = fgetc(F1);
        B = fgetc(F2);
        if (A != B)
        {
            fclose(F1);
            fclose(F2);
            return 0;
        }
        if (A == EOF || B == EOF) break;
    }
    fclose(F1);
    fclose(F2);
    return 1;
}

int main()
{
    int Mode;
    printf_s("Lab 2. Arithmetic compression:\n");
    printf_s("Select operating mode:\n");
    printf_s("1 — Coding\n");
    printf_s("2 — Decoding\n");
    printf_s("> ");
    scanf_s("%d", &Mode);
    clock_t Start_Time = clock();
    if (Mode == 1)
    {
        Encode_File();
        printf_s("Encoding is complete. The resulting file, encoded.txt, has been created.\n");
        FILE* F = fopen("text.txt", "rb");
        if (F)
        {
            fseek(F, 0, SEEK_END);
            long Size1 = ftell(F);
            fclose(F);
            F = fopen("encoded.txt", "rb");
            fseek(F, 0, SEEK_END);
            long Size2 = ftell(F);
            fclose(F);
            printf_s("Compression ratio: %.3f\n", (double)Size2 / (double)Size1);
        }
    }
    else if (Mode == 2)
    {
        Decode_File();
        printf_s("Decoding is complete. The result file decoded.txt has been created.\n");
        if (check_Files_Equal("text.txt", "decoded.txt")) printf_s("Verification: The original text.txt file matches the decoded result in the file decoded.txt\n");
        else printf_s("Check: The original text.txt file does not match the decoded result in the file decoded.txt\n");
    }
    else
    {
        printf_s("Error: Invalid mode selected\n");
        return 1;
    }
    double Time_Spent = (double)(clock() - Start_Time) / CLOCKS_PER_SEC;
    printf_s("Opening hours: %.3f seс\n", Time_Spent);
    return 0;
}