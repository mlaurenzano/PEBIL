/====================/ { 
    isPrint++; 
}

/.*/ { 
    if (isPrint % 3 == 1) 
        isPrint++;
    else if (isPrint % 3 == 2)
        print;
}