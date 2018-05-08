close all;
clear;
BER_ideal = importdata('statistics\BER_ideal.txt','%s');
BER_ideal_pilot = importdata('statistics\BER_ideal_pilot.txt','%s');
BER_detect = importdata('statistics\BER_detect.txt','%s');
SNR = 0:50;

lineWidth=1.5;
for i = 1:15

	figure;
	semilogy(SNR,BER_ideal((i-1)*51+1:i*51),'LineWidth',lineWidth);
	hold on;
	semilogy(SNR,BER_ideal_pilot((i-1)*51+1:i*51),'LineWidth',lineWidth);
	semilogy(SNR,BER_detect((i-1)*51+1:i*51),'LineWidth',lineWidth);
	legend('理想完整信道',...
		'理想导频信道',...
		'估计信道'...
		);
	xlabel('Eb/N0(dB)');
	ylabel('BER');
	title(['选择不同估计信道的误码曲线(numFlow=8,CQI=',num2str(i),')']);
	grid on;

end