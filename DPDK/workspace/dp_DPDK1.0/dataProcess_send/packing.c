//组包格式以子载波数为1200的格式来完成，TotalCarrierNum = 1200 不能随意修改

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>
#include <string.h> 
#include <time.h>
#include "mkl.h"
#include "packing.h"

const int PackHeadLen = 8;
const int SubFrmHeadLen = 32;
const int SymbolHeadLen = 16;
//----------5G-BDMA Frame Structure Look-up Table----------

const int PackHintLut[10][14] = {{0,1,1,1,1,1,1,1,1,1,1,1,1,1},//0 : D
								 {0,0,1,0,0,0,0,0,0,0,0,2,2,2},//1 : S
								 {1,1,1,1,1,1,1,1,1,1,1,1,1,1},//2 : U
								 {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//3 : U
								 {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//4 : U
								 {0,0,0,0,0,0,0,0,0,0,0,0,0,1},//5 : D
								 {0,0,1,0,0,0,0,0,0,0,0,0,0,0},//6 : D
								 {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//7 : D
								 {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//8 : D
								 {0,0,0,0,0,0,0,0,0,0,0,0,0,0} //9 : D	 
								};

const int PackTypeLut[10][14] = {{5,1,1,0,1,1,1,1,1,1,0,1,1,2},//0 : D
								 {3,3,3,3,6,3,7,3,3,3,3,0,0,0},//1 : S
								 {4,4,4,5,4,4,4,4,4,4,5,4,4,6},//2 : U
								 {0,0,0,1,0,0,0,0,0,0,1,0,0,2},//3 : U
								 {0,0,0,1,0,0,0,0,0,0,1,0,0,2},//4 : U
								 {5,3,3,4,3,3,3,3,3,3,4,3,3,2},//5 : D
								 {5,3,3,4,3,3,3,3,3,3,4,3,3,3},//6 : D
								 {5,3,3,4,3,3,3,3,3,3,4,3,3,3},//7 : D
								 {5,3,3,4,3,3,3,3,3,3,4,3,3,3},//8 : D
								 {5,3,3,4,3,3,3,3,3,3,4,3,3,3} //9 : D
								};
			
const int DataIndex[10][14] = {{0,0,1128,2256,2256,3384,4512,5640,6768,7896,9024,9024,10152,11280},//0 : D
							   {0,1200,2400,3528,4728,4728,5928,5928,7128,8328,9528,10728,10728,10728},//1 : S
							   {0,1140,2280,3420,3420,4560,5700,6840,7980,9120,10260,10260,11400,12540},//2 : U
							   {0,1200,2400,3600,3600,4800,6000,7200,8400,9600,10800,10800,12000,13200},//3 : U
							   {0,1200,2400,3600,3600,4800,6000,7200,8400,9600,10800,10800,12000,13200},//4 : U
							   {0,0,1200,2400,2400,3600,4800,6000,7200,8400,9600,9600,10800,12000},//5 : D
							   {0,0,1200,2328,2328,3528,4728,5928,7128,8328,9528,9528,10728,11928},//6 : D
							   {0,0,1200,2400,2400,3600,4800,6000,7200,8400,9600,9600,10800,12000},//7 : D
							   {0,0,1200,2400,2400,3600,4800,6000,7200,8400,9600,9600,10800,12000},//8 : D
							   {0,0,1200,2400,2400,3600,4800,6000,7200,8400,9600,9600,10800,12000} //9 : D  
							  };

const int PilotIndex[10][14] = {{0,0,0,0,1128,1128,1128,1128,1128,1128,1128,2256,2256,2256},//0 : D
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//1 : S
								{0,0,0,0,1140,1140,1140,1140,1140,1140,1140,2280,2280,2280},//2 : U
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240},//3 : U
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240},//4 : U
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240},//5 : D
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240},//6 : D
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240},//7 : D
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240},//8 : D
								{0,0,0,0,1120,1120,1120,1120,1120,1120,1120,2240,2240,2240} //9 : D
							   };

const int PBCHIndex[10][14] = {{0,0,72,144,216,288,360,432,504,576,648,720,792,864},//0 : D
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//1 : S
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//2 : U
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//3 : U
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//4 : U
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//5 : D
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//6 : D
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//7 : D
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0},//8 : D
							   {0,0,0,0,0,0,0,0,0,0,0,0,0,0} //9 : D
							  };

const int PRACHIndex[10][14] = {{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//0 : D
								{0,60,120,180,240,300,360,420,480,540,600,660,720,780},//1 : S
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//2 : U
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//3 : U
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//4 : U
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//5 : D
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//6 : D
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//7 : D
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0},//8 : D
								{0,0,0,0,0,0,0,0,0,0,0,0,0,0} //9 : D
							   };

void packing_S_B(void *arg){

	struct packarg_t packarg = *((struct packarg_t *)arg);
	uint32_t packIndex = packarg.packIndex;
	
	//PDSCH PUSCH
	uint16_t *RealData = packarg.RealData;
	uint16_t *ImagData = packarg.ImagData;
	//DM-RS
	uint16_t *RealPilo = packarg.RealPilo;
	uint16_t *ImagPilo = packarg.ImagPilo;
	//PCFICH  PHICH  PDCCH
	uint16_t *RealPDCCH = packarg.RealPDCCH;
	uint16_t *ImagPDCCH = packarg.ImagPDCCH;
	//PSS
	uint16_t *RealPSS = packarg.RealPSS;
	uint16_t *ImagPSS = packarg.ImagPSS;
	//SSS
	uint16_t *RealSSS = packarg.RealSSS;
	uint16_t *ImagSSS = packarg.ImagSSS;
	//PBCH
	uint16_t *RealPBCH = packarg.RealPBCH;
	uint16_t *ImagPBCH = packarg.ImagPBCH;
	//Channel calibration
	uint16_t *RealCalib = packarg.RealCalib;
	uint16_t *ImagCalib = packarg.ImagCalib;
	
	
	uint32_t LayerNum = packarg.LayerNum;
	uint32_t TotalCarrierNum = packarg.TotalCarrierNum;
	uint32_t CarrierNum = packarg.CarrierNum;
	uint32_t CarrierBegin = packarg.CarrierBegin;
	uint32_t CarrierEnd = packarg.CarrierEnd;
	uint32_t SymbNum = packarg.SymbNum;
	uint32_t FrmIdx = packarg.FrmIdx;
	uint32_t SubFrmIdx = packarg.SubFrmIdx;
	uint32_t SymbIdx = packarg.SymbIdx;
	uint32_t *BfParSel = packarg.BfParSel;
	
	uint16_t *package = packarg.package;
	
	uint16_t *PackHead;
	uint16_t *SubFrmHead;
	uint16_t *SymbolHead;
	uint16_t *SymbolData;
	
	uint32_t SymbDataLen = CarrierNum * LayerNum * 2;
	uint32_t SubFrmDataLen = SymbNum * (SymbolHeadLen + SymbDataLen);
	uint32_t packageLen = PackHeadLen + SubFrmHeadLen + SubFrmDataLen;
	if(SymbIdx == 0) bzero(package,sizeof(uint16_t) * packageLen);

	uint32_t RB_Num = CarrierNum / 12;
	uint32_t PackHint = 0, PackType = 0;
	uint32_t Layers = 0, OritaHint = 0;
	
	uint32_t k = 0, s = 0, m = 0;
	uint32_t isGP = 1;
	uint32_t timecode = time(NULL);
	
	//Head
	if(SymbIdx == 0){
	
		//-----------PackHead-----------8
		PackHead = package;
		PackHead[0] = 0xEB90;//包头指示符
		PackHead[1] = 0x0001;//版本号 类型指示符
		PackHead[2] = 0x8040;//源设备标识符 目标设备标识符
		PackHead[3] = timecode >> 16;//时间码
		PackHead[4] = 0x0000FFFF & timecode;//时间码
		PackHead[5] = packIndex >> 16;//包序控制
		PackHead[6] = 0x0000FFFF & packIndex;//包序控制
		PackHead[7] = packageLen;//包长度
		
		//-----------SubFrmHead-----------32
		SubFrmHead = package + PackHeadLen;
		
		SubFrmHead[0] = FrmIdx << 4;//FrmIdx  WORD0[15:4]
		SubFrmHead[0] = SubFrmHead[0] | SubFrmIdx;//SubFrmIdx  WORD0[3:0]
		
		SubFrmHead[1] = SymbNum << 12;//SybNum  WORD1[15:12]
		SubFrmHead[1] = SubFrmHead[1] | RB_Num;//RB_Num  WORD1[11:0]
		
		SubFrmHead[2] = LayerNum << 8;//BfLayers  WORD2[15:8]
		//BfParSel  WORD2[7:0]
		
		SubFrmHead[3] = SymbolHeadLen << 8;//SybHeadLen  WORD3[15:8]
		SubFrmHead[3] = SubFrmHead[3] | ((int)ceil(RB_Num / 16.0));//OritaBitmapLen  WORD3[7:0]
		
		SubFrmHead[4] = SymbDataLen >> 8;//SybDataLen  WORD4
		SubFrmHead[5] = (SymbDataLen & 0x000000FF) << 8;//SybDataLen  WORD5[15:8]
		
		SubFrmHead[5] = SubFrmHead[5] | (SubFrmDataLen >> 16);//SubFrmDataLen[31:16]  WORD5[7:0]
		SubFrmHead[6] = SubFrmDataLen & 0x0000FFFF;//SubFrmDataLen[15:0] WORD6
		
		//CarrierNum  WORD7[15:8]
		//CarrierIdx  WORD7[7:0]
		
		SubFrmHead[8] = BfParSel[0] << 8;//BfParSel_L1  Word8[15:8]
		SubFrmHead[8] = SubFrmHead[8] | BfParSel[1];//BfParSel_L2  Word8[7:0]
		SubFrmHead[9] = BfParSel[2] << 8;//BfParSel_L3  Word9[15:8]
		SubFrmHead[9] = SubFrmHead[9] | BfParSel[3];//BfParSel_L4  Word9[7:0]
		SubFrmHead[10] = BfParSel[4] << 8;//BfParSel_L5  Word10[15:8]
		SubFrmHead[10] = SubFrmHead[10] | BfParSel[5];//BfParSel_L6  Word10[7:0]
		SubFrmHead[11] = BfParSel[6] << 8;//BfParSel_L7  Word11[15:8]
		SubFrmHead[11] = SubFrmHead[11] | BfParSel[7];//BfParSel_L8  Word11[7:0]
		
		SubFrmHead[12] = TotalCarrierNum;
		SubFrmHead[13] = CarrierBegin;
		SubFrmHead[14] = CarrierEnd;

		for(int i = 15; i < 32; i++){
			SubFrmHead[i] = 0;//reserved  WORD12~31
		}
	}
	
	//Symbol
	SymbolHead = (package + PackHeadLen + SubFrmHeadLen) + (SymbolHeadLen + CarrierNum * LayerNum * 2) * SymbIdx;
	PackHint = PackHintLut[SubFrmIdx][SymbIdx];
	PackType = PackTypeLut[SubFrmIdx][SymbIdx];
	
	switch (PackHint){
	case 0 ://单一业务数据包
		isGP = 0;
		switch (PackType){
		case 0 ://PUSCH
			printf("\nError:SubFrmIdx %d is Upload\n",SubFrmIdx);
			break;
		case 1 ://上行 DM-RS
			printf("\nError:SubFrmIdx %d is Upload\n",SubFrmIdx);
			break;
		case 2 ://SRS
			printf("\nError:SubFrmIdx %d is Upload\n",SubFrmIdx);
			break;
		case 3 ://PDSCH
			Layers = LayerNum;
			OritaHint = 1;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = DataIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						SymbolData[s] = ImagData[k];
						SymbolData[s + 1] = RealData[k];
						k++;
					}
				}
			}
			break;
		case 4 ://下行 DM-RS
			Layers = LayerNum;
			OritaHint = 1;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = PilotIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						SymbolData[s] = ImagPilo[k];
						SymbolData[s + 1] = RealPilo[k];
						k++;
					}
				}
			}
			break;
		case 5 ://PDCCH
			Layers = LayerNum;
			OritaHint = 1;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						SymbolData[s] = ImagPDCCH[k];
						SymbolData[s + 1] = RealPDCCH[k];
						k++;
					}
				}
			}
			break;
		case 6 ://校准接收通道信号
			Layers = LayerNum;
			OritaHint = 1;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						SymbolData[s] = RealCalib[k];
						SymbolData[s + 1] = ImagCalib[k];
						k++;
					}
				}
			}
			break;
		case 7 ://校准发送通道信号
			Layers = LayerNum;
			OritaHint = 1;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						SymbolData[s] = RealCalib[k];
						SymbolData[s + 1] = ImagCalib[k];
						k++;
					}
				}
			}
			break;
		default :
			printf("\nError:Invalid PackType\n");
			break;
		}
		break;
	case 1 ://复合业务数据包
		isGP = 0;
		switch (PackType){
		case 0 ://PBCH + 下行 DMRS
			Layers = 0x0100 | LayerNum;
			OritaHint = 2;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			SymbolHead[4] = SymbolHead[4] & 0xFFFE;
			SymbolHead[5] = SymbolHead[5] & 0x07FF;
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			m = 0;
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 564 && nc < 637){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealPBCH[k];
							SymbolData[s + 1] = ImagPBCH[k];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealPilo[m];
							SymbolData[s + 1] = ImagPilo[m];
							m++;
						}
					}
				}
			}
			break;
		case 1 ://PBCH + PDSCH
			Layers = 0x0100 | LayerNum;
			OritaHint = 2;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			SymbolHead[4] = SymbolHead[4] & 0xFFFE;
			SymbolHead[5] = SymbolHead[5] & 0x07FF;
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = PBCHIndex[SubFrmIdx][SymbIdx];
			m = DataIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 564 && nc < 637){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealPBCH[k];
							SymbolData[s + 1] = ImagPBCH[k];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealData[m];
							SymbolData[s + 1] = ImagData[m];
							m++;
						}
					}
				}
			}
			break;
		case 2 ://SSS + PDSCH
			Layers = 0x0100 | LayerNum;
			OritaHint = 2;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			SymbolHead[4] = SymbolHead[4] & 0xFFFE;
			SymbolHead[5] = SymbolHead[5] & 0x07FF;
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			m = DataIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 564 && nc < 637){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealSSS[k];
							SymbolData[s + 1] = ImagSSS[k];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealData[m];
							SymbolData[s + 1] = ImagData[m];
							m++;
						}
					}
				}
			}
			break;
		case 3 ://PSS + PDSCH
			Layers = 0x0100 | LayerNum;
			OritaHint = 2;
			//OritaBitmap  WORD2~15
			for(int i = 2; i < 16; i++){
				SymbolHead[i] = 0xffff;
			}
			SymbolHead[4] = SymbolHead[4] & 0xFFFE;
			SymbolHead[5] = SymbolHead[5] & 0x07FF;
			//Data
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			m = DataIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 564 && nc < 637){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealPSS[k];
							SymbolData[s + 1] = ImagPSS[k];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							SymbolData[s] = RealData[m];
							SymbolData[s + 1] = ImagData[m];
							m++;
						}
					}
				}
			}
			break;
		case 4 ://PRACH + PUSCH
			printf("\nError:SubFrmIdx %d is Upload\n",SubFrmIdx);
			break;
		case 5 ://PRACH + 上行DM-RS
			printf("\nError:SubFrmIdx %d is Upload\n",SubFrmIdx);
			break;
		case 6 ://PRACH + SRS
			printf("\nError:SubFrmIdx %d is Upload\n",SubFrmIdx);
			break;
		default :
			printf("\nError:Invalid PackType\n");
			break;
		}
		break;
	case 2://GP
		break;
	default ://ERROR
		printf("\nError:Invalid PackHint\n");
		break;
	
	}
	
	//-----------SymbolHead-----------16
	if(isGP != 1){
		SymbolHead[0] = SymbIdx << 12;//SybIdx  WORD0[15:12]
		SymbolHead[0] = SymbolHead[0] | (PackHint << 8);//PackHint  WORD0[11:8]
		SymbolHead[0] = SymbolHead[0] | (PackType << 4);//PackType  WORD0[7:4]
	
		SymbolHead[0] = SymbolHead[0] | (Layers >> 12);//Layers[15:12]  WORD0[3:0]		
		SymbolHead[1] = (Layers & 0x00000FFF) << 4;//Layers[11:0]  WORD1[15:4]
	
		SymbolHead[1] = SymbolHead[1] | OritaHint;//OritaHint  WORD1[3:0]
	}

}

void unpacking_B_S(void *arg){

	struct packarg_t packarg = *((struct packarg_t *)arg);
	
	uint32_t packIndex = packarg.packIndex;
	
	//PDSCH PUSCH
	uint16_t *RealData = packarg.RealData;
	uint16_t *ImagData = packarg.ImagData;
	//DM-RS
	uint16_t *RealPilo = packarg.RealPilo;
	uint16_t *ImagPilo = packarg.ImagPilo;
	//SRS
	uint16_t *RealSRS = packarg.RealSRS;
	uint16_t *ImagSRS = packarg.ImagSRS;
	//PRACH
	uint16_t *RealPRACH = packarg.RealPRACH;
	uint16_t *ImagPRACH = packarg.ImagPRACH;
	
	
	uint32_t LayerNum = packarg.LayerNum;
	uint32_t CarrierNum = packarg.CarrierNum;
	uint32_t TotalCarrierNum;
	uint32_t CarrierBegin;
	uint32_t CarrierEnd;
	uint32_t SymbNum = packarg.SymbNum;
	uint32_t SubFrmIdx;
	uint32_t SymbIdx = packarg.SymbIdx;
	
	uint16_t *package = packarg.package;
	
	uint16_t *PackHead;
	uint16_t *SubFrmHead;
	uint16_t *SymbolHead;
	uint16_t *SymbolData;
	
	uint32_t SymbDataLen = CarrierNum * LayerNum * 2;
	uint32_t SubFrmDataLen = SymbNum * (SymbolHeadLen + SymbDataLen);
	uint32_t packageLen = PackHeadLen + SubFrmHeadLen + SubFrmDataLen;
	
	uint32_t PackHint = 0, PackType = 0;
	uint32_t Layers = 0, OritaHint = 0;
	
	uint32_t k = 0, s = 0, m = 0;
	
	//-----------PackHead-----------8
	PackHead = package;
	
		
	//-----------SubFrmHead-----------32
	SubFrmHead = package + PackHeadLen;
	
	SubFrmIdx = SubFrmHead[0] & 0x000F; //SubFrmIdx  WORD0[3:0]
	TotalCarrierNum = SubFrmHead[12];
	CarrierBegin = SubFrmHead[13];
	CarrierEnd = SubFrmHead[14];
	//-----------SymbolHead-----------16
	SymbolHead = (package + PackHeadLen + SubFrmHeadLen) + (SymbolHeadLen + CarrierNum * LayerNum * 2) * SymbIdx;
	
	//Symbol
	SymbolHead = (package + PackHeadLen + SubFrmHeadLen) + (SymbolHeadLen + CarrierNum * LayerNum * 2) * SymbIdx;
	
	PackHint = PackHintLut[SubFrmIdx][SymbIdx];
	PackType = PackTypeLut[SubFrmIdx][SymbIdx];
	switch (PackHint){
	case 0 ://单一业务数据包
		switch (PackType){
		case 0 ://PUSCH
			SymbolData = SymbolHead + SymbolHeadLen;
			k = DataIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						ImagData[k] = SymbolData[s];
						RealData[k] = SymbolData[s + 1];
						k++;
					}
				}
			}
			break;
		case 1 ://上行 DM-RS
			SymbolData = SymbolHead + SymbolHeadLen;
			k = PilotIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						ImagPilo[k] = SymbolData[s];
						RealPilo[k] = SymbolData[s + 1];
						k++;
					}
				}
			}
			break;
		case 2 ://SRS
			SymbolData = SymbolHead + SymbolHeadLen;
			k = 0;
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					for(int nl = 0; nl < LayerNum; nl++){
						s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
						ImagSRS[k] = SymbolData[s];
						RealSRS[k] = SymbolData[s + 1];
						k++;
					}
				}
			}
			break;
		case 3 ://PDSCH
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 4 ://下行 DM-RS
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 5 ://PDCCH
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 6 ://校准接收通道信号
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 7 ://校准发送通道信号
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		default :
			printf("\nError:Invalid PackType\n");
			break;
		}
		break;
	case 1 ://复合业务数据包
		switch (PackType){
		case 0 ://PBCH + 下行 DMRS
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 1 ://PBCH + PDSCH
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 2 ://SSS + PDSCH
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 3 ://PSS + PDSCH
			printf("\nError:SubFrmIdx %d is Download\n",SubFrmIdx);
			break;
		case 4 ://PRACH + PUSCH
			SymbolData = SymbolHead + SymbolHeadLen;
			k = PRACHIndex[SubFrmIdx][SymbIdx];
			m = DataIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 0 && nc < 60){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							RealPRACH[k] = SymbolData[s];
							ImagPRACH[k] = SymbolData[s + 1];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							RealData[m] = SymbolData[s];
							ImagData[m] = SymbolData[s + 1];
							m++;
						}
					}
				}
			}
			break;
		case 5 ://PRACH + 上行DM-RS
			SymbolData = SymbolHead + SymbolHeadLen;
			k = PRACHIndex[SubFrmIdx][SymbIdx];
			m = PilotIndex[SubFrmIdx][SymbIdx];
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 0 && nc < 60){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							RealPRACH[k] = SymbolData[s];
							ImagPRACH[k] = SymbolData[s + 1];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							RealPilo[m] = SymbolData[s];
							ImagPilo[m] = SymbolData[s + 1];
							m++;
						}
					}
				}
			}
			break;
		case 6 ://PRACH + SRS
			SymbolData = SymbolHead + SymbolHeadLen;
			k = PRACHIndex[SubFrmIdx][SymbIdx];
			m = 0;
			for(int nc = 0; nc < TotalCarrierNum; nc++){
				if(nc >= CarrierBegin && nc < CarrierEnd){
					if(nc >= 0 && nc < 60){
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							RealPRACH[k] = SymbolData[s];
							ImagPRACH[k] = SymbolData[s + 1];
							k++;
						}
					}
					else{
						for(int nl = 0; nl < LayerNum; nl++){
							s = (nc - CarrierBegin) * LayerNum * 2 + nl * 2;
							RealSRS[m] = SymbolData[s];
							ImagSRS[m] = SymbolData[s + 1];
							m++;
						}
					}
				}
			}
			break;
		default :
			printf("\nError:Invalid PackType\n");
			break;
		}
		break;
	case 2://GP
		break;
	default ://ERROR
		printf("\nError:Invalid PackHint\n");
		break;
	
	}
	
}
